import io
import importlib.util
import os
from pathlib import Path
import tempfile
import threading
import unittest
import urllib.request
import urllib.error
import wave
from http.server import ThreadingHTTPServer

spec = importlib.util.spec_from_file_location('server', Path(__file__).resolve().parents[1] / 'raspberry/server.py')
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


def audio(rate=16000):
    buffer = io.BytesIO()
    with wave.open(buffer, 'wb') as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(b'\0\0' * 3200)
    return buffer.getvalue()


class AudioTests(unittest.TestCase):
    def test_correct_format(self):
        server.validate_wav(audio())

    def test_wrong_rate(self):
        with self.assertRaises(ValueError):
            server.validate_wav(audio(8000))

    def test_truncated(self):
        with self.assertRaises(ValueError):
            server.validate_wav(audio()[:-20])


@unittest.skipIf(os.name == 'nt', 'Ejecutable simulado para Linux/macOS')
class HttpTests(unittest.TestCase):
    def test_transcription_and_rejection(self):
        with tempfile.TemporaryDirectory() as folder:
            fake = Path(folder) / 'fake-whisper'
            fake.write_text('#!/usr/bin/env python3\nimport sys\nfrom pathlib import Path\nout=sys.argv[sys.argv.index("-of")+1]\nPath(out+".txt").write_text("Hola, ¿cómo estás?",encoding="utf-8")\n')
            fake.chmod(0o755)
            httpd = ThreadingHTTPServer(('127.0.0.1', 0), server.Handler)
            httpd.cli = fake
            httpd.model = Path(folder) / 'model.bin'
            httpd.inference_lock = threading.Lock()
            worker = threading.Thread(target=httpd.serve_forever, daemon=True)
            worker.start()
            base = f'http://127.0.0.1:{httpd.server_port}'
            try:
                with urllib.request.urlopen(base + '/health') as response:
                    self.assertEqual(response.status, 200)
                request = urllib.request.Request(base + '/transcribe', data=audio(), headers={'Content-Type': 'audio/wav'})
                with urllib.request.urlopen(request) as response:
                    self.assertEqual(response.read().decode(), 'Hola, ¿cómo estás?')
                bad = urllib.request.Request(base + '/transcribe', data=audio(8000))
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(bad)
                self.assertEqual(error.exception.code, 400)
                httpd.inference_lock.acquire()
                try:
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        urllib.request.urlopen(request)
                    self.assertEqual(error.exception.code, 503)
                finally:
                    httpd.inference_lock.release()
            finally:
                httpd.shutdown()
                httpd.server_close()
                worker.join()


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env python3
"""Servidor local de prueba: WAV -> whisper-cli -> texto UTF-8. Sin pip."""
import argparse
import io
import json
import socket
import subprocess
import tempfile
import threading
import time
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

MAX_BYTES = 16000 * 2 * 15 + 4096


def validate_wav(data):
    with wave.open(io.BytesIO(data), 'rb') as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getcomptype()) != (1, 2, 16000, 'NONE'):
            raise ValueError('Se requiere WAV PCM mono, 16 bits, 16000 Hz')
        frames = wav.getnframes()
        if not 1600 <= frames <= 16000 * 15:
            raise ValueError('Duracion permitida: 0.1 a 15 segundos')
        if len(wav.readframes(frames)) != frames * 2:
            raise ValueError('Audio truncado')


class Handler(BaseHTTPRequestHandler):
    def reply(self, status, text, content_type='text/plain; charset=utf-8'):
        body = text.encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path != '/health':
            return self.reply(404, 'Ruta inexistente')
        self.reply(200, json.dumps({'status': 'ok', 'model': self.server.model.name}), 'application/json')

    def do_POST(self):
        if self.path != '/transcribe':
            return self.reply(404, 'Ruta inexistente')
        if self.headers.get('Transfer-Encoding'):
            return self.reply(400, 'Usa Content-Length')
        try:
            size = int(self.headers.get('Content-Length', '0'))
        except ValueError:
            return self.reply(400, 'Content-Length invalido')
        if not 44 < size <= MAX_BYTES:
            return self.reply(413, 'Tamano de audio no permitido')
        if not self.server.inference_lock.acquire(blocking=False):
            return self.reply(503, 'Raspberry ocupada; intenta de nuevo')
        try:
            self.connection.settimeout(20)
            data = self.rfile.read(size)
            if len(data) != size:
                raise ValueError('Envio incompleto')
            validate_wav(data)
            with tempfile.TemporaryDirectory(prefix='voz-local-') as folder:
                audio = Path(folder) / 'audio.wav'
                output = Path(folder) / 'result'
                audio.write_bytes(data)
                start = time.monotonic()
                result = subprocess.run([
                    str(self.server.cli), '-m', str(self.server.model),
                    '-f', str(audio), '-l', 'es', '-t', '4',
                    '-otxt', '-of', str(output),
                ], capture_output=True, text=True, timeout=150)
                if result.returncode:
                    print(result.stderr[-2000:], flush=True)
                    return self.reply(500, 'Error en Whisper; revisa la terminal de la Raspberry')
                text = output.with_suffix('.txt').read_text(encoding='utf-8').strip()
                # Registra tiempos, sin conservar el audio ni imprimir lo dicho.
                print(f'Transcripcion: {time.monotonic() - start:.2f} s', flush=True)
                self.reply(200, text)
        except (ValueError, wave.Error, EOFError) as error:
            self.reply(400, str(error))
        except subprocess.TimeoutExpired:
            self.reply(504, 'Whisper supero 150 segundos')
        except (socket.timeout, TimeoutError):
            self.reply(408, 'Tiempo de recepcion agotado')
        except (OSError, UnicodeError):
            self.reply(500, 'Error local; revisa rutas y archivos de Whisper')
        finally:
            self.server.inference_lock.release()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='0.0.0.0')
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--cli', type=Path, required=True)
    parser.add_argument('--model', type=Path, required=True)
    args = parser.parse_args()
    if not args.cli.is_file() or not args.model.is_file():
        parser.error('No se encuentra whisper-cli o el modelo; ejecuta scripts/setup_pi.sh')
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    server.cli, server.model = args.cli.resolve(), args.model.resolve()
    server.inference_lock = threading.Lock()
    print(f'Servidor en puerto {args.port}. Ctrl+C para detener.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()

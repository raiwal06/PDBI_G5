# Voz local: ESP32-S3 + Raspberry Pi

Prototipo para tu Raspberry Pi 4 de 8 GB con Debian 13 aarch64, ESP32-S3-CAM
N16R8, microfono INMP441 y OLED I2C 128x64. El codigo se edita en la laptop,
se versiona en GitHub y se ejecuta en la Raspberry / ESP32 respectivamente.

## Que hace esta version

Envia `r` desde el monitor serie: el ESP32 graba cinco segundos, envia un WAV
por HTTP a la Raspberry y muestra el texto devuelto en paginas de cinco lineas.
Cada pagina dura cinco segundos. No graba mientras transcribe o muestra texto.
Todavia no incluye deteccion de voz, subtitulos continuos ni la camara.
Whisper puede equivocarse, especialmente con silencio o ruido. Esta es una
prueba de ingenieria, no una transcripcion clinica validada.

Se usa HTTP en esta primera prueba en lugar del WebSocket inicialmente propuesto
para simplificar la verificacion de una frase completa. El procesamiento es local;
internet solo se requiere para descargar herramientas y modelos. Inicialmente ambos
equipos deben conectarse a la misma red de 2.4 GHz, con comunicacion entre clientes.
La configuracion de Raspberry como punto de acceso queda para una segunda etapa.

## Archivos

- `firmware/voz_local/voz_local.ino`: se carga al ESP32 desde la laptop por USB.
- `firmware/voz_local/config.example.h`: ejemplo de configuracion privada.
- `raspberry/server.py`: recibe WAV y ejecuta whisper-cli, sin paquetes pip.
- `scripts/setup_pi.sh`: instala y compila Whisper solamente en la Raspberry.
- `scripts/run_pi.sh`: inicia el servidor, ejecutandolo desde cualquier carpeta.
- `tests/test_server.py`: pruebas del protocolo con un ejecutable simulado.

## 1. Laptop: abrir y subir a GitHub

Descomprime el proyecto, abre la carpeta `voz-local` con VS Code y abre una terminal
en esa carpeta. Crea en GitHub un repositorio VACIO llamado `voz-local`, sin README,
gitignore ni licencia (el proyecto ya incluye los dos primeros). Puede ser privado;
en ese caso la Raspberry necesitara acceso mediante SSH o credenciales de GitHub.

Ejecuta lo siguiente, reemplazando TU_USUARIO por tu usuario real:

```bash
git init -b main
git add .
git commit -m "Prototipo de voz local ESP32 y Raspberry"
git remote add origin https://github.com/TU_USUARIO/voz-local.git
git push -u origin main
```

Si Git pide identidad, configura `git config user.name` y `git config user.email`
con tus datos. La autenticacion a GitHub requiere SSH, un gestor de credenciales
o token; no funciona con la contrasena normal de la cuenta. No pegues tokens en el codigo.

## 2. Raspberry: descargar y preparar

Estos comandos van en la RASPBERRY; sustituye TU_USUARIO:

```bash
cd ~
git clone https://github.com/TU_USUARIO/voz-local.git
cd voz-local
bash scripts/setup_pi.sh
bash scripts/run_pi.sh
```

La compilacion puede tardar varios minutos. El script descarga `tiny` multilingue,
no `tiny.en`. La copia de whisper.cpp y su modelo quedan en `vendor/`, excluido de Git.
No necesitas repetir la instalacion para cada cambio del servidor.
Si ya instalaste Whisper en otra carpeta, esta instalacion crea una copia independiente.

Con el servidor ejecutandose, abre una SEGUNDA terminal en la Raspberry:

```bash
curl http://127.0.0.1:8080/health
hostname -I
```

El primero debe devolver `status: ok`. Anota la IP de la Raspberry dentro de tu
red Wi-Fi para configurar SERVER_URL. Manten abierta la terminal del servidor.
Si la IP cambia, actualiza SERVER_URL; puedes reservarla en tu router mas adelante.

## 3. Laptop: configurar y cargar el ESP32

En Arduino IDE instala el paquete **esp32 by Espressif Systems, version 3.x** y
la biblioteca **U8g2 by oliver** desde sus gestores. Usa una version estable que
incluya `ESP_I2S.h`; este sketch no es para Arduino-ESP32 2.x.

Abre `firmware/voz_local/voz_local.ino`. Copia `config.example.h` como `config.h`
en la MISMA carpeta (puedes hacerlo con el explorador de archivos). Edita:

```cpp
const char* WIFI_SSID = "Nombre de tu red";
const char* WIFI_PASSWORD = "Clave de tu red";
const char* SERVER_URL = "http://IP_DE_LA_RASPBERRY:8080/transcribe";
```

`config.h` queda fuera de Git. No publiques tu clave en `config.example.h`.
En Herramientas selecciona la placa correspondiente a tu fabricante; si no trae
definicion propia, prueba **ESP32S3 Dev Module**, Flash Size **16 MB**, PSRAM
**OPI PSRAM** para el modulo N16R8. Estas opciones dependen de la placa concreta.
El sketch puede funcionar con memoria interna si la asignacion de PSRAM falla.
No modifica ni inicia la camara. Verifica en el esquema de tu placa que los GPIO
indicados estan disponibles; las posiciones fisicas varian entre fabricantes.

| INMP441 | ESP32-S3 |
|---|---|
| VDD | 3V3 |
| GND | GND |
| L/R | GND (canal izquierdo) |
| WS | GPIO 42 |
| SCK | GPIO 41 |
| SD | GPIO 14 |

| OLED | ESP32-S3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 1 |
| SCL | GPIO 2 |

Controlador OLED AUN NO CONFIRMADO: por defecto el sketch prueba SH1106 en 0x3C.
Si tu pantalla es SSD1306, cambia `OLED_SH1106` a `0` en config.h.
La direccion tambien es configurable. El tamano de 1.3 pulgadas no confirma
por si solo el controlador. Si la OLED queda vacia, identifica controlador y
direccion antes de atribuir el problema a Whisper.

Carga el sketch con el ESP32 conectado por el puerto USB de programacion.
Selecciona el puerto correcto en Arduino IDE y abre el monitor serie a **115200**.
Para usar el conector USB nativo, puede ser necesario activar **USB CDC On Boot**;
para un puente USB-UART se usa su puerto serie. Depende de tu placa y del conector.

## 4. Prueba completa

1. Raspberry: deja corriendo `bash scripts/run_pi.sh`.
2. Laptop: comprueba `http://IP_DE_LA_RASPBERRY:8080/health` en el navegador.
3. Monitor serie: envia `r` y habla CUANDO aparezca `Habla ahora: 5 s`.
4. Espera a que se muestre el texto en la OLED y en el monitor serie.
5. Envia otra `r` para repetir. El ejemplo hace cortes fijos de cinco segundos.

La primera prueba permite medir el tiempo de inferencia, que aparece en la
terminal de la Raspberry. El cliente espera hasta 180 segundos y el servidor
limita la inferencia a 150. El modelo se carga por frase; una version posterior
puede mantenerlo en memoria para reducir la demora.

Para probar un audio independiente del ESP32:

```bash
ffmpeg -i grabacion_original.m4a -ar 16000 -ac 1 -c:a pcm_s16le prueba.wav
curl --fail-with-body -H 'Content-Type: audio/wav' --data-binary @prueba.wav http://IP_DE_LA_RASPBERRY:8080/transcribe
```

Usa una grabacion de entre 0.1 y 15 segundos. El servidor rechaza formatos distintos
de PCM mono 16 kHz/16 bits y no conserva los archivos de audio tras procesarlos.
El endpoint de prototipo no incorpora autenticacion ni TLS: usalo en tu red local
de prueba y no abras el puerto 8080 en internet.

## 5. Cambios posteriores: laptop -> GitHub -> Raspberry

En la laptop, despues de editar:

```bash
git add .
git commit -m "Describe aqui tu cambio"
git push
```

En la Raspberry, detén el servidor con Ctrl+C y ejecuta:

```bash
cd ~/voz-local
git pull --ff-only
bash scripts/run_pi.sh
```

GitHub no ejecuta ni actualiza automaticamente la Raspberry: `git pull` descarga
los cambios, y debes reiniciar el proceso para aplicarlos. Los cambios de firmware
requieren volver a cargar el sketch al ESP32 por USB desde la laptop.

## Validacion disponible

```bash
python3 -m unittest discover -s tests -v
```

Las pruebas verifican formato de audio y el envio HTTP con un Whisper simulado;
no verifican reconocimiento de voz real ni sustituyen una prueba con tu hardware.
El firmware requiere compilacion con Arduino IDE y prueba fisica. Esta entrega no
se ha compilado para la placa ni ejecutado sobre tu Raspberry.

## Referencias tecnicas

- https://github.com/ggml-org/whisper.cpp
- https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2s.html
- https://github.com/olikraus/u8g2/wiki/u8g2setupcpp

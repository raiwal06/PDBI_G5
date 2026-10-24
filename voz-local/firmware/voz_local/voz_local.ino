// Arduino-ESP32 3.x, biblioteca U8g2. Prueba manual por frases.
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP_I2S.h>
#include "config.h"

#if OLED_SH1106
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, 2, 1);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, 2, 1);
#endif
I2SClass mic;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t SAMPLES = SAMPLE_RATE * 5;
constexpr size_t WAV_SIZE = 44 + SAMPLES * 2;
uint8_t* wavBuffer = nullptr;

void status(const String& text) {
  Serial.println(text);
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawUTF8(0, 14, text.c_str());
  oled.sendBuffer();
}

void put16(size_t offset, uint16_t value) {
  wavBuffer[offset] = value & 255;
  wavBuffer[offset + 1] = value >> 8;
}
void put32(size_t offset, uint32_t value) {
  put16(offset, value & 65535);
  put16(offset + 2, value >> 16);
}

void wavHeader() {
  memcpy(wavBuffer, "RIFF", 4);
  put32(4, WAV_SIZE - 8);
  memcpy(wavBuffer + 8, "WAVEfmt ", 8);
  put32(16, 16);
  put16(20, 1);
  put16(22, 1);
  put32(24, SAMPLE_RATE);
  put32(28, SAMPLE_RATE * 2);
  put16(32, 2);
  put16(34, 16);
  memcpy(wavBuffer + 36, "data", 4);
  put32(40, SAMPLES * 2);
}

bool recordAudio() {
  mic.setPins(41, 42, -1, 14); // BCLK, WS, salida no usada, SD entrada
  if (!mic.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) return false;
  int32_t raw[256];
  // Descarta el arranque del microfono antes de indicar que puede hablar.
  for (int i = 0; i < 8; ++i) mic.readBytes((char*)raw, sizeof(raw));
  status("Habla ahora: 5 s");
  size_t written = 0;
  while (written < SAMPLES) {
    size_t wanted = SAMPLES - written;
    if (wanted > 256) wanted = 256;
    size_t bytes = mic.readBytes((char*)raw, wanted * sizeof(int32_t));
    if (bytes != wanted * sizeof(int32_t)) {
      mic.end();
      return false;
    }
    for (size_t i = 0; i < wanted; ++i) {
      // INMP441: muestra con signo alineada a la izquierda en slot de 32 bits.
      int32_t sample = raw[i] >> 16;
      put16(44 + (written + i) * 2, (uint16_t)(int16_t)sample);
    }
    written += wanted;
  }
  mic.end(); // No acumular muestras viejas durante la transcripcion.
  return true;
}

// Paginacion por caracteres UTF-8, sin cortar una tilde a mitad de secuencia.
void showText(String text) {
  Serial.println(text);
  text.replace("\r", " ");
  text.replace("\n", " ");
  if (text.length() == 0) text = "Sin texto reconocido";
  size_t pos = 0;
  while (pos < text.length()) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x12_tf);
    for (int row = 0; row < 5 && pos < text.length(); ++row) {
      String line;
      while (pos < text.length()) {
        size_t next = pos + 1;
        while (next < text.length() && (((uint8_t)text[next] & 0xC0) == 0x80)) ++next;
        String candidate = line + text.substring(pos, next);
        if (oled.getUTF8Width(candidate.c_str()) > 128 && line.length()) break;
        line = candidate;
        pos = next;
      }
      oled.drawUTF8(0, 12 + row * 12, line.c_str());
    }
    oled.sendBuffer();
    delay(5000);
  }
}

bool connectWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

void setup() {
  Serial.begin(115200);
  oled.setI2CAddress(OLED_ADDRESS << 1);
  oled.begin();
  status("Iniciando...");
  wavBuffer = (uint8_t*)ps_malloc(WAV_SIZE);
  if (!wavBuffer) wavBuffer = (uint8_t*)malloc(WAV_SIZE);
  if (!wavBuffer) {
    status("Error de memoria");
    while (true) delay(1000);
  }
  wavHeader();
  WiFi.mode(WIFI_STA);
  status(connectWifi() ? "WiFi conectado" : "Revisa WiFi");
  Serial.println("Envia r para grabar una frase de cinco segundos.");
}

void loop() {
  if (!Serial.available() || Serial.read() != 'r') { delay(10); return; }
  if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
    status("Sin WiFi"); return;
  }
  if (!recordAudio()) { status("Error microfono"); return; }
  status("Transcribiendo...");
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(180000);
  if (!http.begin(client, SERVER_URL)) { status("URL incorrecta"); return; }
  http.addHeader("Content-Type", "audio/wav");
  int code = http.POST(wavBuffer, WAV_SIZE);
  String response = code > 0 ? http.getString() : HTTPClient::errorToString(code);
  http.end();
  if (code == 200) showText(response);
  else {
    Serial.printf("Error HTTP: %d\n", code);
    Serial.println(response);
    status("Error; mira Serial");
  }
  Serial.println("Envia r para otra frase.");
}

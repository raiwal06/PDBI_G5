#pragma once
// Copia este archivo como config.h y modifica los valores en tu laptop.
// config.h queda excluido de Git para no publicar tu clave Wi-Fi.
const char* WIFI_SSID = "TU_RED_2_4_GHZ";
const char* WIFI_PASSWORD = "TU_CLAVE";
const char* SERVER_URL = "http://192.168.1.100:8080/transcribe";
// Hipotesis inicial: SH1106. Si tu controlador es SSD1306, usa 0.
#define OLED_SH1106 1
#define OLED_ADDRESS 0x3C

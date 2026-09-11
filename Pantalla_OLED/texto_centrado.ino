#include <U8g2lib.h>
#include <Wire.h>

#define SDA_PIN 1
#define SCL_PIN 2

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

const char *parrafo = "Hola Mundo este es un texto largo que se desplaza y queda centrado en la pantalla del OLED";

#define MAX_LINEAS 10
String lineas[MAX_LINEAS];
int numLineas = 0;
int alturaLinea;
int yOffset;

void armarLineas() {
  char buffer[200];
  strcpy(buffer, parrafo);

  char *palabra = strtok(buffer, " ");
  String lineaActual = "";

  while (palabra != NULL) {
    String pruebaLinea = lineaActual.length() == 0 ? String(palabra) : lineaActual + " " + palabra;

    if (u8g2.getStrWidth(pruebaLinea.c_str()) > 128) {
      lineas[numLineas++] = lineaActual;
      lineaActual = palabra;
    } else {
      lineaActual = pruebaLinea;
    }
    palabra = strtok(NULL, " ");
  }
  if (lineaActual.length() > 0) {
    lineas[numLineas++] = lineaActual;
  }
}

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.setFont(u8g2_font_5x7_tr);

  alturaLinea = u8g2.getAscent() - u8g2.getDescent() + 1;
  armarLineas();
  yOffset = 64; // arranca justo debajo de la pantalla
}

void loop() {
  u8g2.clearBuffer();

  for (int i = 0; i < numLineas; i++) {
    int y = yOffset + i * alturaLinea;
    if (y > -alturaLinea && y < 64 + alturaLinea) {
      int ancho = u8g2.getStrWidth(lineas[i].c_str());
      int x = (128 - ancho) / 2; // centra cada linea horizontalmente
      u8g2.drawStr(x, y, lineas[i].c_str());
    }
  }

  u8g2.sendBuffer();

  yOffset--; // mueve todo el bloque hacia arriba

  int alturaTotal = numLineas * alturaLinea;
  if (yOffset < -alturaTotal) {
    yOffset = 64; // cuando ya salio todo por arriba, reinicia desde abajo
  }

  delay(50);
}

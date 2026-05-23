#ifndef LOGO_H
#define LOGO_H

#include <lacop.h>

extern Adafruit_SSD1306 display;

// Converte um byte de XBM para o formato Adafruit
byte reverseBits(byte b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

void logo() {
  display.clearDisplay();

  // Converte e desenha o bitmap
  byte convertedBitmap[uff_lacop_height * ((uff_lacop_width + 7) / 8)];

  int size = uff_lacop_height * ((uff_lacop_width + 7) / 8);
  for (int i = 0; i < size; i++) {
    convertedBitmap[i] = reverseBits(uff_lacop_bits[i]);
  }

  display.drawBitmap(0, 0, convertedBitmap, uff_lacop_width, uff_lacop_height, WHITE);
  display.display();
}

#endif
// Поиск адреса дисплея на шине I2C. Один файл, чтобы диагностические
// прошивки не зависели от драйвера oled.cpp.
#pragma once

#include <Wire.h>

inline uint8_t oledProbeAddr() {
  static const uint8_t CANDIDATES[] PROGMEM = {0x3C, 0x3D};
  for (uint8_t i = 0; i < sizeof(CANDIDATES); i++) {
    uint8_t a = pgm_read_byte(CANDIDATES + i);
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) return a;
  }
  return 0;
}
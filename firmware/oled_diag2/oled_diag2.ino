// Проверка грубых операций: сначала полная очистка памяти дисплея, затем
// одна крупная фигура. Никакого шрифта и мелких деталей — только крупные
// заливки, чтобы было видно, работает запись вообще или нет.
//
// Порядок работы:
//   1. 0xAE — выключить дисплей (память при этом не очищается)
//   2. записать 8 страниц по 128 байт нулей — полная очистка
//   3. 0xAF — включить дисплей
//   4. нарисовать крест: вертикальная полоса 8 px и горизонтальная 8 px
//
// Если после шага 3 экран станет чёрным — запись работает, и можно рисовать.
// Если экран остался в шуме — запись не работает, и дальше крутить init.

#include <Wire.h>

#include "probe.h"

static uint8_t g_addr;

static void raw(const uint8_t *d, uint8_t n, bool isData) {
  Wire.beginTransmission(g_addr);
  Wire.write(isData ? 0x40 : 0x00);
  Wire.write(d, n);
  Wire.endTransmission();
}

static void cmd1(uint8_t c) {
  raw(&c, 1, false);
}

static void clearGddram() {
  uint8_t zeros[128];
  memset(zeros, 0, sizeof(zeros));
  for (uint8_t p = 0; p < 8; p++) {
    uint8_t seq[4] = {0x00, (uint8_t)(0xB0 | p), 0x00, 0x10};
    raw(seq, 4, false);
    Wire.beginTransmission(g_addr);
    Wire.write((uint8_t)0x40);
    Wire.write(zeros, 128);
    Wire.endTransmission();
  }
}

static void fillPage(uint8_t p, uint8_t pattern) {
  uint8_t buf[128];
  memset(buf, pattern, sizeof(buf));
  uint8_t seq[4] = {0x00, (uint8_t)(0xB0 | p), 0x00, 0x10};
  raw(seq, 4, false);
  Wire.beginTransmission(g_addr);
  Wire.write((uint8_t)0x40);
  Wire.write(buf, 128);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
  Wire.begin();
  Wire.setClock(100000);
  Wire.setWireTimeout(25000, true);

  g_addr = oledProbeAddr();
  if (!g_addr) {
    Serial.println(F("display not found"));
    return;
  }
  Serial.print(F("addr 0x"));
  Serial.println(g_addr, HEX);

  // Инициализация по datasheet SSD1315.
  const uint8_t init[] PROGMEM = {
      0xAE,        // выключить
      0xD5, 0x80,  // делитель частоты
      0xA8, 0x3F,  // мультиплексор 1/64
      0xD3, 0x00,  // смещение
      0x40,        // стартовая строка
      0xAD, 0x02,  // внутренний IREF
      0xA1,        // разворот по горизонтали
      0xC8,        // разворот COM
      0xDA, 0x12,  // конфигурация COM
      0x81, 0x9F,  // контраст
      0xD9, 0x22,  // предзаряд
      0xDB, 0x35,  // VCOMH
      0x8D, 0x14,  // зарядный насос
      0x20, 0x00,  // горизонтальный режим адресации
      0xA4,        // отображать GDDRAM
      0xA6,        // нормальная полярность
      0x2E,        // выключить прокрутку
  };
  for (uint8_t i = 0; i < sizeof(init); i++) {
    uint8_t b = pgm_read_byte(init + i);
    cmd1(b);
  }

  cmd1(0xAE);        // выключить перед очисткой
  clearGddram();     // полная очистка
  cmd1(0xAF);        // включить
  delay(100);

  Serial.println(F("cleared, drawing cross"));
  delay(1500);

  // Крест: вертикальная полоса по центру и горизонтальная по центру.
  fillPage(3, 0xFF);
  for (uint8_t p = 0; p < 8; p++) {
    uint8_t buf[128];
    memset(buf, 0, sizeof(buf));
    if (p != 3) {
      for (uint8_t x = 58; x < 70; x++) buf[x] = 0xFF;
    }
    uint8_t seq[4] = {0x00, (uint8_t)(0xB0 | p), 0x00, 0x10};
    raw(seq, 4, false);
    Wire.beginTransmission(g_addr);
    Wire.write((uint8_t)0x40);
    Wire.write(buf, 128);
    Wire.endTransmission();
  }
  Serial.println(F("cross drawn"));
}

void loop() {}
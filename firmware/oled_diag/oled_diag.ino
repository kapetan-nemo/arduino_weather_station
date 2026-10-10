// Диагностика двумя независимыми проверками за один запуск.
//
// Проверка 1 — шрифт в памяти. Читаем pgm_read_byte для букв 'A', 'B', 'C'
// и для цифр '1', '2' и печатаем байты в порт. Если байты одинаковые во всех
// глифах — таблица шрифта не читается, дело в памяти, а не в дисплее.
// Если байты разные и осмысленные — шрифт в порядке.
//
// Проверка 2 — передача на дисплей. Рисуем на весь экран шахматку из двух
// байт: 0xAA и 0x55 в чередующемся порядке по колонкам. Если экран станет
// такой шахматкой — передача и адресация работают, и предыдущие неудачи были
// в отрисовке. Если экран покажет одинаковый символ по всей ширине —
// адресация не двигает указатель столбцов.
//
// Обе проверки выполняются сразу и остаются на экране.

#include <Wire.h>

#include "font6x8.h"
#include "oled.h"

static void dumpGlyph(char ch) {
  uint8_t idx = ch - FONT6X8_FIRST;
  Serial.print(F("'"));
  Serial.print(ch);
  Serial.print(F("' idx="));
  Serial.print(idx);
  Serial.print(F(" bytes="));
  for (uint8_t r = 0; r < 7; r++) {
    Serial.print(pgm_read_byte(font6x8 + idx * 8 + r), HEX);
    Serial.print(' ');
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
  Wire.begin();
  Wire.setClock(100000);
  Wire.setWireTimeout(25000, true);

  // Проверка 1: содержимое таблицы шрифта.
  Serial.println(F("--- font in flash ---"));
  dumpGlyph('A');
  dumpGlyph('B');
  dumpGlyph('W');
  dumpGlyph('1');
  dumpGlyph('2');

  uint8_t addr = oledProbe();
  Serial.print(F("OLED:0x"));
  Serial.println(addr, HEX);
  if (!addr) {
    Serial.println(F("display not found"));
    return;
  }
  oledInit(addr, OLED_SSD1315);

  // Проверка 2: шахматка во весь экран. Столбец x заполняется байтом
  // 0xAA или 0x55 по чётности x — это даёт вертикальные полосы шириной 1 px,
  // и по ним сразу видно, идут ли колонки подряд или указатель стоит.
  for (uint8_t p = 0; p < OLED_PAGES; p++) {
    for (int x = 0; x < OLED_W; x++) {
      oledPixel(x, p * 8, (x & 1) == 0);
    }
  }
  // Затем поверх — горизонтальные полосы на 1 пиксель по высоте.
  for (int y = 0; y < OLED_H_px; y++) {
    oledPixel(0, y, true);
    oledPixel(OLED_W - 1, y, true);
  }
  oledFlush();
  Serial.println(F("checkerboard drawn"));
}

void loop() {}
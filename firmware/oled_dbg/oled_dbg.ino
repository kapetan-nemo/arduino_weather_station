// Пошаговая диагностика дисплея с командами из монитора, чтобы не
// перезаливать прошивку на каждый тест.
//
// Команды (по одной букве, в мониторе 115200 бод):
//
//   c  — очистить всё: 8 страниц нулями, должен стать чёрный экран
//   1  — залить страницу 0 белым (полоса 8 px вверху)
//   2  — залить страницу 1 белым
//   3  — залить страницу 2 белым
//   4  — залить страницу 3 белым
//   5  — залить страницу 4 белым
//   6  — залить страницу 5 белым
//   7  — залить страницу 6 белым
//   8  — залить страницу 7 белым
//   v  — вертикальная полоса шириной 1 px в столбцах 0..7, все страницы
//   h  — горизонтальная полоса высотой 1 px в строке 0, все страницы
//   a  — переключить режим адресации: постраничный / горизонтальный
//   k  — переключить контраст 0x81 между 0x7F и 0x9F
//   r  — переключить внутренний IREF 0xAD: 0x00, 0x02, 0x1E
//   x  — выключить дисплей (0xAE)
//   y  — включить дисплей (0xAF)
//   s  — вывести состояние
//
// Каждая команда печатает в порт, что именно отправлено на дисплей.

#include <Wire.h>

#include "probe.h"

static uint8_t g_addr = 0;
static uint8_t g_contrast = 0x7F;
static uint8_t g_iref = 0x00;
static bool g_horizontal = false;

static void cmd(const uint8_t *d, uint8_t n) {
  Wire.beginTransmission(g_addr);
  Wire.write((uint8_t)0x00);
  Wire.write(d, n);
  Wire.endTransmission();
}

static void cmd1(uint8_t c) {
  Wire.beginTransmission(g_addr);
  Wire.write((uint8_t)0x00);
  Wire.write(c);
  Wire.endTransmission();
}

// Запись одной страницы: сначала команды адресации, затем 128 байт данных.
static void writePage(uint8_t p, const uint8_t *buf) {
  Wire.beginTransmission(g_addr);
  Wire.write((uint8_t)0x00);
  if (g_horizontal) {
    Wire.write((uint8_t)0x21);  // диапазон столбцов
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)0x7F);
    Wire.write((uint8_t)0x22);  // диапазон страниц
    Wire.write(p);
    Wire.write(p);
  } else {
    Wire.write((uint8_t)(0xB0 | p));  // адрес страницы
    Wire.write((uint8_t)0x00);        // младший разряд столбца
    Wire.write((uint8_t)0x10);        // старший разряд столбца
  }
  Wire.write((uint8_t)0x40);  // теперь данные
  Wire.write(buf, 128);
  uint8_t err = Wire.endTransmission();
  if (err) {
    Serial.print(F("ERR="));
    Serial.println(err);
  }
}

static void clearAll() {
  uint8_t zeros[128];
  memset(zeros, 0, sizeof(zeros));
  for (uint8_t p = 0; p < 8; p++) writePage(p, zeros);
}

static void fillPage(uint8_t p, uint8_t pattern) {
  uint8_t buf[128];
  memset(buf, pattern, sizeof(buf));
  writePage(p, buf);
}

static void applyContrast() {
  uint8_t s[2] = {0x81, g_contrast};
  cmd(s, 2);
}

static void applyIref() {
  uint8_t s[2] = {0xAD, g_iref};
  cmd(s, 2);
}

static void applyAddressing() {
  uint8_t s[2] = {0x20, (uint8_t)(g_horizontal ? 0x00 : 0x02)};
  cmd(s, 2);
  Serial.print(F("addr mode = "));
  Serial.println(g_horizontal ? "horizontal" : "page");
}

// Запись страницы кусками по chunk байт. Если частая передача не проходит,
// а кусочная проходит — дело в длине транзакции, то есть в целостности
// сигнала на проводах.
static void writePageChunked(uint8_t p, uint8_t chunk) {
  uint8_t buf[128];
  memset(buf, 0, sizeof(buf));
  Wire.beginTransmission(g_addr);
  Wire.write((uint8_t)0x00);
  Wire.write((uint8_t)(0xB0 | p));
  Wire.write((uint8_t)0x00);
  Wire.write((uint8_t)0x10);
  Wire.write((uint8_t)0x40);
  if (Wire.endTransmission()) {
    Serial.println(F("addr ERR"));
    return;
  }
  for (uint8_t off = 0; off < 128; off += chunk) {
    Wire.beginTransmission(g_addr);
    Wire.write((uint8_t)0x40);
    Wire.write(buf + off, chunk);
    uint8_t err = Wire.endTransmission();
    if (err) {
      Serial.print(F("chunk "));
      Serial.print(off);
      Serial.print(F(" ERR="));
      Serial.println(err);
      return;
    }
  }
  Serial.print(F("chunked ok, chunk="));
  Serial.println(chunk);
}

static void applyInit() {
  const uint8_t init[] PROGMEM = {
      0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
      0xA1, 0xC8, 0xDA, 0x12, 0xD9, 0x22, 0xDB, 0x35,
      0x8D, 0x14, 0xA4, 0xA6, 0x2E,
  };
  for (uint8_t i = 0; i < sizeof(init); i++) cmd1(pgm_read_byte(init + i));
  applyIref();
  applyContrast();
  applyAddressing();
  cmd1(0xAF);
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
    Serial.println(F("DISPLAY NOT FOUND"));
    return;
  }
  Serial.print(F("addr 0x"));
  Serial.println(g_addr, HEX);
  applyInit();
  Serial.println(F("ready: c 1-8 v h a k r x y s"));
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'c') {
      clearAll();
      Serial.println(F("c: cleared, 8 pages of zeros"));
    } else if (c >= '1' && c <= '8') {
      uint8_t p = c - '1';
      fillPage(p, 0xFF);
      Serial.print(F("page "));
      Serial.print(p);
      Serial.println(F(" = 0xFF"));
    } else if (c == 'v') {
      uint8_t buf[128];
      memset(buf, 0, sizeof(buf));
      memset(buf, 0xFF, 8);  // столбцы 0..7
      for (uint8_t p = 0; p < 8; p++) writePage(p, buf);
      Serial.println(F("v: columns 0..7 white on all pages"));
    } else if (c == 'h') {
      uint8_t buf[128];
      for (uint8_t p = 0; p < 8; p++) {
        memset(buf, 0, sizeof(buf));
        buf[0] = 0x01;  // бит 0 — верхняя строка страницы
        writePage(p, buf);
      }
      Serial.println(F("h: row 0 white on all pages"));
    } else if (c == 'a') {
      g_horizontal = !g_horizontal;
      applyAddressing();
      clearAll();
    } else if (c == 'k') {
      g_contrast = g_contrast == 0x7F ? 0x9F : 0x7F;
      applyContrast();
      Serial.print(F("contrast = 0x"));
      Serial.println(g_contrast, HEX);
    } else if (c == 'r') {
      g_iref = g_iref == 0x00 ? 0x02 : (g_iref == 0x02 ? 0x1E : 0x00);
      applyIref();
      Serial.print(F("iref = 0x"));
      Serial.println(g_iref, HEX);
    } else if (c == 'x') {
      cmd1(0xAE);
      Serial.println(F("display off"));
    } else if (c == 'y') {
      cmd1(0xAF);
      Serial.println(F("display on"));
    } else if (c == 'w') {
      // Заливка всех страниц кусками по 16 байт.
      uint8_t buf[128];
      memset(buf, 0xFF, sizeof(buf));
      Wire.beginTransmission(g_addr);
      Wire.write((uint8_t)0x00);
      Wire.write((uint8_t)(0xB0));
      Wire.write((uint8_t)0x00);
      Wire.write((uint8_t)0x10);
      Wire.write((uint8_t)0x40);
      Wire.endTransmission();
      for (uint8_t off = 0; off < 128; off += 16) {
        Wire.beginTransmission(g_addr);
        Wire.write((uint8_t)0x40);
        Wire.write(buf + off, 16);
        Wire.endTransmission();
      }
      Serial.println(F("w: page0 chunked 16B = 0xFF"));
    } else if (c == 'W') {
      writePageChunked(0, 16);
      writePageChunked(1, 16);
    } else if (c == 'z') {
      // Низкая частота шины: 12.5 кГц, сильно ниже нормы.
      Wire.setClock(12500);
      Serial.println(F("z: bus clock = 12500"));
    } else if (c == 'Z') {
      Wire.setClock(100000);
      Serial.println(F("Z: bus clock = 100000"));
    } else if (c == 's') {
      Serial.print(F("addr=0x"));
      Serial.print(g_addr, HEX);
      Serial.print(F(" contrast=0x"));
      Serial.print(g_contrast, HEX);
      Serial.print(F(" iref=0x"));
      Serial.print(g_iref, HEX);
      Serial.print(F(" mode="));
      Serial.println(g_horizontal ? "horiz" : "page");
    }
  }
}
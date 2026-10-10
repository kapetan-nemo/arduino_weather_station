#include "oled.h"

#include <Wire.h>

#include "font6x8.h"

static uint8_t g_addr = 0;
static OledController g_ctrl = OLED_SSD1306;
static bool g_ready = false;

// Кадровый буфер: 128 байт на страницу, бит 0 — верхний пиксель строки.
static uint8_t fb[OLED_PAGES][OLED_W];
// Бит n установлен, если страница n изменилась с прошлого oledFlush().
static uint8_t dirty = 0;

// Растровые команды SSD1306 и SH1106 совпадают, кроме набора заряда:
// у SSD1306 это 0x8D 0x14, у SH1106 — 0xAD 0x8B, и обе команды чужому
// контроллеру безвредны, поэтому общий набор используется для обоих.
static const uint8_t INIT_COMMON[] PROGMEM = {
    0xAE,        // выключить дисплей
    0xD5, 0x80,  // делитель тактовой частоты, осциллятор по умолчанию
    0xA8, 0x3F,  // мультиплексор 1/64
    0xD3, 0x00,  // смещение вывода
    0x40,        // начальная строка 0
    0xA1,        // разворот по горизонтали: столбец 127 -> SEG0
    0xC8,        // разворот COM-выводов снизу вверх
    0xDA, 0x12,  // конфигурация COM для матрицы 128x64
    0x81, 0x7F,  // контраст
    0xD9, 0xF1,  // период предзаряда
    0xDB, 0x40,  // уровень VCOMH
    0xA4,        // отображать содержимое GDDRAM
    0xA6,        // нормальная полярность
    0x2E,        // выключить прокрутку
};

// Последующие команды различаются у чипов, поэтому идут отдельными массивами.
//
// SSD1306: 0x20 0x00 — горизонтальный режим адресации, 0x8D 0x14 — насос включён.
// SH1106:  0x20 0x00, зарядный насос 0xAD 0x8B.
// SSD1315: отличий, критичных для вывода, нет. GDDRAM ровно 128x64, смещения
// столбцов не требуется. В регистре 0x20 младший бит выбирает постраничный
// режим, поэтому 0x20 0x02. Команда 0xAD (внутренний IREF) необязательна и
// не отправляется: её битовые поля в datasheet не совпадают с SSD1306.
static const uint8_t INIT_SSD1306[] PROGMEM = {0x20, 0x00, 0x8D, 0x14};
static const uint8_t INIT_SH1106[] PROGMEM = {0x20, 0x00, 0xAD, 0x8B};
static const uint8_t INIT_SSD1315[] PROGMEM = {0x20, 0x02, 0x8D, 0x14};

static void cmd(const uint8_t *seq, uint8_t n) {
  Wire.beginTransmission(g_addr);
  Wire.write((uint8_t)0x00);  // control byte: Co=0, D/C=0 -> команда
  for (uint8_t i = 0; i < n; i++) Wire.write(pgm_read_byte(seq + i));
  Wire.endTransmission();
}

static void cmd1(uint8_t c) {
  Wire.beginTransmission(g_addr);
  Wire.write((uint8_t)0x00);
  Wire.write(c);
  Wire.endTransmission();
}

// Возвращает true, если устройство подтвердило адрес.
static bool ack(uint8_t a) {
  Wire.beginTransmission(a);
  return Wire.endTransmission() == 0;
}

uint8_t oledProbe() {
  static const uint8_t CANDIDATES[] PROGMEM = {0x3C, 0x3D};
  for (uint8_t i = 0; i < sizeof(CANDIDATES); i++) {
    uint8_t a = pgm_read_byte(CANDIDATES + i);
    if (ack(a)) return a;
  }
  return 0;
}

bool oledInit(uint8_t addr, OledController ctrl) {
  g_addr = addr;
  g_ctrl = ctrl;
  g_ready = false;
  if (!addr) return false;

  cmd(INIT_COMMON, sizeof(INIT_COMMON));
  switch (ctrl) {
    case OLED_SSD1306:
      cmd(INIT_SSD1306, sizeof(INIT_SSD1306));
      break;
    case OLED_SH1106:
      cmd(INIT_SH1106, sizeof(INIT_SH1106));
      break;
    default:
      cmd(INIT_SSD1315, sizeof(INIT_SSD1315));
      break;
  }
  cmd1(0xAF);  // включить дисплей
  // Заметка: подтверждение записи настроек получить нельзя — у этих
  // контроллеров нет регистра статуса. Признак успеха один: на экране
  // видна очищенная память и нарисованный текст.

  oledClear();
  oledFlush();
  g_ready = true;
  return true;
}

void oledSetController(OledController ctrl) {
  if (ctrl == g_ctrl) return;
  uint8_t a = g_addr;
  g_ctrl = ctrl;
  oledInit(a, ctrl);
}

OledController oledController() { return g_ctrl; }
uint8_t oledAddress() { return g_addr; }
bool oledReady() { return g_ready; }

void oledClear() {
  memset(fb, 0, sizeof(fb));
  dirty = 0xFF;
}

uint8_t oledDirtyPages() {
  uint8_t n = 0;
  for (uint8_t p = 0; p < OLED_PAGES; p++)
    if (dirty & (1 << p)) n++;
  return n;
}

void oledFlush() {
  if (!g_addr) return;
  // У SH1106 память шире на 2 байта, видимая область начинается со столбца 2.
  // У SSD1306 и SSD1315 ровно 128 столбцов, смещения нет.
  uint8_t col = (g_ctrl == OLED_SH1106) ? 2 : 0;
  for (uint8_t p = 0; p < OLED_PAGES; p++) {
    if (!(dirty & (1 << p))) continue;
    dirty &= ~(1 << p);  // сбросить сразу: неудача не должна делить кадр навечно
    Wire.beginTransmission(g_addr);
    Wire.write((uint8_t)0x00);                    // control byte -> команда
    Wire.write((uint8_t)0x20);                    // режим адресации
    Wire.write((uint8_t)0x00);                    // горизонтальный
    Wire.write((uint8_t)0x21);                    // диапазон столбцов
    Wire.write((uint8_t)col);
    Wire.write((uint8_t)(col + OLED_W - 1));
    Wire.write((uint8_t)0x22);                    // диапазон страниц
    Wire.write((uint8_t)p);
    Wire.write((uint8_t)p);
    Wire.write((uint8_t)0x40);                    // control byte -> данные
    Wire.write(fb[p], OLED_W);
    if (Wire.endTransmission() != 0) dirty |= 1 << p;
  }
}

void oledPixel(int x, int y, bool on) {
  if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H_px) return;
  uint8_t bit = 1 << (y & 7);
  uint8_t &cell = fb[y >> 3][x];
  uint8_t want = on ? (cell | bit) : (uint8_t)(cell & ~bit);
  if (want == cell) return;  // состояние не изменилось — страницу не трогаем
  cell = want;
  dirty |= 1 << (y >> 3);
}

void oledFillRect(int x, int y, int w, int h, bool on) {
  for (int j = 0; j < h; j++) oledLine(x, y + j, x + w - 1, y + j, on);
}

void oledLine(int x0, int y0, int x1, int y1, bool on) {
  // Алгоритм Брезенхэма: dy хранится со знаком минус, чтобы условия
  // переходов не требовали знаковых делений внутри цикла.
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    oledPixel(x0, y0, on);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void oledRect(int x, int y, int w, int h, bool on) {
  if (w <= 0 || h <= 0) return;
  oledLine(x, y, x + w - 1, y, on);
  oledLine(x, y + h - 1, x + w - 1, y + h - 1, on);
  oledLine(x, y, x, y + h - 1, on);
  oledLine(x + w - 1, y, x + w - 1, y + h - 1, on);
}

static uint8_t glyphIndex(uint8_t ch) {
  if (ch == 0xB0) return FONT6X8_DEG_INDEX;
  if (ch >= FONT6X8_FIRST && ch < FONT6X8_FIRST + FONT6X8_COUNT)
    return ch - FONT6X8_FIRST;
  return '?' - FONT6X8_FIRST;  // всё неопределённое -> "?"
}

uint8_t oledTextWidth(const char *s, uint8_t scale) {
  if (scale < 1) scale = 1;
  uint8_t n = 0;
  for (; *s; s++) n++;
  return n ? (uint8_t)(n * 6 * scale - scale) : 0;
}

static void drawText(int x, int y, const char *s, uint8_t scale, bool on) {
  if (scale < 1) scale = 1;
  for (; *s; s++) {
    uint8_t idx = glyphIndex((uint8_t)*s);
    for (uint8_t r = 0; r < 7; r++) {
      uint8_t bits = pgm_read_byte(font6x8 + idx * 8 + r);
      if (!bits) continue;
      for (uint8_t c = 0; c < 5; c++) {
        if (!(bits & (1 << c))) continue;
        for (uint8_t dy = 0; dy < scale; dy++)
          for (uint8_t dx = 0; dx < scale; dx++)
            oledPixel(x + c * scale + dx, y + r * scale + dy, on);
      }
    }
    x += 6 * scale;
  }
}

void oledText(int x, int y, const char *s, uint8_t scale) {
  drawText(x, y, s, scale, true);
}

void oledTextInv(int x, int y, const char *s, uint8_t scale) {
  if (scale < 1) scale = 1;
  uint8_t n = oledTextWidth(s, scale);
  // Заливка на всю ширину строки вместе с завершающим зазором символа.
  oledFillRect(x, y, n + scale, 8 * scale, true);
  drawText(x, y, s, scale, false);
}

uint8_t oledFmt(char *buf, float v, uint8_t decimals) {
  if (decimals > 3) decimals = 3;
  uint8_t n = 0;
  // Предел: 4 значащие цифры целой части плюс дробная часть.
  static const uint16_t LIMIT[4] = {9999, 999, 99, 9};
  bool bad = (v != v) || (v > LIMIT[decimals]);
  if (bad) {
    buf[0] = '-'; buf[1] = '-'; buf[2] = 0;
    return 2;
  }
  bool neg = v < 0;
  if (neg) v = -v;
  static const uint32_t POW10[4] = {1, 10, 100, 1000};
  uint32_t scaled = (uint32_t)(v * POW10[decimals] + 0.5f);
  uint32_t ip = scaled / POW10[decimals];
  uint32_t fp = scaled % POW10[decimals];

  if (neg && scaled) buf[n++] = '-';  // "-0.0" не печатаем
  char tmp[5];
  uint8_t t = 0;
  do {
    tmp[t++] = '0' + (ip % 10);
    ip /= 10;
  } while (ip);
  while (t) buf[n++] = tmp[--t];

  if (decimals) {
    buf[n++] = '.';
    for (uint8_t d = decimals; d > 0; d--) {
      buf[n++] = '0' + (fp / POW10[d - 1]) % 10;
    }
  }
  buf[n] = 0;
  return n;
}
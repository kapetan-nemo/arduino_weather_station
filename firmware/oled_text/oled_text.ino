// Проверка отрисовки: фраза "ты пус" и сердечко после слова "пус".
//
// Кириллица: встроенный шрифт Adafruit_GFX содержит только ASCII, поэтому
// нужные буквы заданы здесь вручную как битовые маски.
//
// Формат глифа: 8 байт, по одному на строку. Байт — строка глифа, бит x —
// пиксель в столбце x. Используются 7 столбцов из 8: столбец 7 оставлен
// пустым и служит зазором между символами. Восьмой байт — пустая строка
// интерлиньяжа.

#include <SPI.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Глифы 7x7, столбец 7 не используется.
static const uint8_t G_T[8] PROGMEM = {0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00};  // т
static const uint8_t G_Y[8] PROGMEM = {0x43, 0x43, 0x43, 0x7B, 0x43, 0x43, 0x7B, 0x00};  // ы
static const uint8_t G_P[8] PROGMEM = {0x7F, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00};  // п
static const uint8_t G_U[8] PROGMEM = {0x63, 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x38, 0x00};  // у
static const uint8_t G_S[8] PROGMEM = {0x3E, 0x63, 0x60, 0x60, 0x60, 0x63, 0x3E, 0x00};  // с
static const uint8_t G_HEART[8] PROGMEM = {0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08, 0x00, 0x00};

// Последовательность фразы: две буквы, пробел, три буквы, пробел, сердце.
static void drawGlyph(int x, int y, const uint8_t *g, uint8_t scale) {
  for (uint8_t r = 0; r < 7; r++) {
    uint8_t bits = pgm_read_byte(g + r);
    for (uint8_t c = 0; c < 7; c++) {
      if (!(bits & (1 << c))) continue;
      for (uint8_t dy = 0; dy < scale; dy++)
        for (uint8_t dx = 0; dx < scale; dx++)
          display.drawPixel(x + c * scale + dx, y + r * scale + dy, SSD1306_WHITE);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
  Wire.setClock(100000);
  Wire.setWireTimeout(25000, true);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("display.begin FAILED at 0x3C"));
    return;
  }
  Serial.println(F("began at 0x3C"));

  drawPhrase();
}

// Прямая отправка одной команды. display.begin() уже отправил свои 0xA1 и
// 0xC8, но ни begin(), ни display() их не переустанавливают, поэтому команду
// можно поменять после инициализации без перезаливки прошивки.
static void rawCmd(uint8_t c) {
  Wire.beginTransmission(0x3C);
  Wire.write((uint8_t)0x00);
  Wire.write(c);
  Wire.endTransmission();
}

static void drawPhrase() {

  // Масштаб 2: один символ 14x14 пикселей, зазор 2 пикселя — итого шаг 16.
  // Раскладка по горизонтали: т(2) ы(18) пробел(34) п(50) у(66) с(82)
  // пробел(98) сердце(114). Сердце кончается на 128, то есть ровно у края.
  const uint8_t S = 2;
  const int Y = 8;
  const int STEP = 16;

  drawGlyph(2, Y, G_T, S);
  drawGlyph(2 + STEP, Y, G_Y, S);
  drawGlyph(2 + STEP * 3, Y, G_P, S);
  drawGlyph(2 + STEP * 4, Y, G_U, S);
  drawGlyph(2 + STEP * 5, Y, G_S, S);
  drawGlyph(2 + STEP * 7, Y, G_HEART, S);

  display.display();
  Serial.println(F("phrase drawn"));
}

// Перерисовка после смены ориентации: буфер не менялся, а адресация на
// дисплее теперь другая, поэтому нужно отправить кадр заново.
static void redraw() {
  display.display();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '1') {
      rawCmd(0xA1);
      Serial.println(F("segremap 0xA1"));
    } else if (c == '0') {
      rawCmd(0xA0);
      Serial.println(F("segremap 0xA0"));
    } else if (c == '8') {
      rawCmd(0xC8);
      Serial.println(F("comscan 0xC8"));
    } else if (c == 'c') {
      rawCmd(0xC0);
      Serial.println(F("comscan 0xC0"));
    } else if (c == 'r') {
      redraw();
      Serial.println(F("redrawn"));
    }
  }
}
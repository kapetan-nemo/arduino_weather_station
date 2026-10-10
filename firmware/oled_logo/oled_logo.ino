// Проверка дисплея: логотип Adafruit, затем голландская фраза с сердечком.
//
// Логотип берётся из splash.h, который входит в библиотеку Adafruit_SSD1306:
// splash1_width = 82, splash1_height = 64. Рисуется через drawBitmap() из
// Adafruit_GFX, обычным вызовом, а не зашитым в begin().
//
// Текст набирается встроенным шрифтом Adafruit_GFX — он латинский, поэтому
// для голландской фразы ничего рисовать по битам не нужно. Исключение —
// сердечко, такого знака в шрифте нет.

#include <SPI.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "splash.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Сердечко: во встроенном шрифте Adafruit_GFX такого знака нет, поэтому
// рисуется битами. 8 байт, байт — строка глифа, бит x — столбец x.
// Столбец 6 пустой и служит зазором. Строки 0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08
// дают две доли сверху, сужение книзу и остриё.
static const uint8_t G_HEART[8] PROGMEM = {0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08, 0x00, 0x00};

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

static void drawLogo() {
  display.clearDisplay();
  display.drawBitmap((SCREEN_WIDTH - splash1_width) / 2,
                     (SCREEN_HEIGHT - splash1_height) / 2, splash1_data,
                     splash1_width, splash1_height, SSD1306_WHITE);
  display.display();
}

// «Je bent een kleine Pus» и сердечко после слова «Pus».
//
// Голландский текст набран встроенным шрифтом Adafruit_GFX: он латинский,
// кириллица не нужна, поэтому буквы строить по битам не требуется.
// Шрифт масштаба 1 занимает 6 пикселей по ширине на знак, 7 строк по высоте.
// В строке 10 знаков = 60 пикселей, по центру экрана это x = 34.
//
// Сердечка во встроенном шрифте нет, оно по-прежнему рисуется битами:
// G_HEART, 7 столбцов, масштаб 2 = 14 пикселей по ширине и высоте.
static void drawPhrase() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(34, 8);
  display.print("Je bent een");
  display.setCursor(24, 30);
  display.print("kleine Pus");

  drawGlyph(90, 29, G_HEART, 2);

  display.display();
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

  // Сразу при включении: логотип, затем через 4 секунды голландская фраза.
  // Дальше содержимое можно переключать из монитора:
  //   1 — только логотип
  //   2 — только «Je bent een kleine Pus» и сердечко
  //   3 — логотип, затем через 4 секунды фраза и сердечко
  drawLogo();
  Serial.println(F("logo only"));
  delay(4000);
  drawPhrase();
  Serial.println(F("phrase only"));
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '1') {
      drawLogo();
      Serial.println(F("logo only"));
    } else if (c == '2') {
      drawPhrase();
      Serial.println(F("phrase only"));
    } else if (c == '3') {
      drawLogo();
      delay(4000);
      drawPhrase();
      Serial.println(F("logo then phrase"));
    }
  }
}
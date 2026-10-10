// Проверка: работает ли дисплей через библиотеку Adafruit_SSD1306.
//
// Библиотека ставится в ~/Documents/Arduino/libraries, ставится arduino-cli
// автоматически через library.properties. Зависимость Adafruit_GFX_Library
// тоже должна быть установлена.
//
// Что делает прошивка: инициализация через библиотеку, затем заливка всего
// экрана нулём, затем через GFX рисуются крупные фигуры — рамка по краям,
// горизонтальная полоса в середине и вертикальная полоса слева. Если фигуры
// на месте и в правильном порядке, значит последовательность команд рабочая.

#include <SPI.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// SSD1306_SWITCHCAPVCC — модуль питается от внешнего 3.3V, что как раз наш
// случай: дисплей подключён к выводу 3.3V платы.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
  Wire.setClock(100000);
  Wire.setWireTimeout(25000, true);

  Serial.print(F("probe 0x3C = "));
  Wire.beginTransmission(0x3C);
  Serial.println(Wire.endTransmission());
  Serial.print(F("probe 0x3D = "));
  Wire.beginTransmission(0x3D);
  Serial.println(Wire.endTransmission());

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("display.begin FAILED at 0x3C"));
    // Вторая попытка на другом адресе.
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("display.begin FAILED at 0x3D too"));
      return;
    }
    Serial.println(F("began at 0x3D"));
  } else {
    Serial.println(F("began at 0x3C"));
  }

  display.clearDisplay();

  // Рамка по краям всего экрана.
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);

  // Горизонтальная полоса: в середине по вертикали.
  display.fillRect(0, 28, SCREEN_WIDTH, 8, SSD1306_WHITE);

  // Вертикальная полоса: слева, шириной 8 пикселей.
  display.fillRect(0, 0, 8, SCREEN_HEIGHT, SSD1306_WHITE);

  // Текст библиотечным шрифтом: должен быть читаемым, это проверка GFX.
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 8);
  display.print("SSD1306 128x64");
  display.setCursor(16, 18);
  display.print("GFX font OK");

  display.display();
  Serial.println(F("frame sent"));
}

void loop() {}
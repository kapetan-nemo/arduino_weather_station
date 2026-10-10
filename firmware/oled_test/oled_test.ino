// Тест дисплея: рисует эталонные узоры, по которым видно, где ломается вывод.
//
// Порядок проверки по строкам сверху вниз. Каждая строка на экране —
// отдельная проверка, чтобы по картинке сразу понять, что не так:
//
//   строка 0: сплошная заливка      — работает ли передача страниц вообще
//   строка 1: текст "ABCDEFGH" x1   — читается ли шрифт и порядок колонок
//   строка 2: текст "12345678" x2   — работает ли удвоение масштаба
//   строка 3: шахматка             — совпадает ли адресация страниц
//   строка 4: рамка и заливка левой половины — нет ли сдвига колонок
//   строка 5: счётчик "TEST nnn"   — меняется ли содержимое экрана
//
// Если строка 5 не меняется — не работает перерисовка. Если весь текст
// сдвинут вправо ровно на 2 пикселя или обрезан слева — это SH1106,
// в мониторе надо нажать 'h'.

#include <Wire.h>

#include "oled.h"

static uint16_t tick = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
  Wire.begin();
  Wire.setClock(100000);
  Wire.setWireTimeout(25000, true);

  uint8_t addr = oledProbe();
  Serial.print(F("OLED:0x"));
  Serial.println(addr, HEX);
  if (!addr) {
    Serial.println(F("display not found"));
    return;
  }
  oledInit(addr, OLED_SSD1315);

  // Строка 0: сплошная заливка.
  oledFillRect(0, 0, OLED_W, 8, true);

  // Строка 1: текст масштабом 1.
  oledText(0, 16, "ABCDEFGH", 1);

  // Строка 2: текст масштабом 2.
  oledText(0, 24, "12345678", 2);

  // Строка 3: шахматка по всей странице.
  for (int y = 48; y < 56; y++)
    for (int x = 0; x < OLED_W; x++)
      oledPixel(x, y, ((x + y) & 1) != 0);

  // Строка 4: рамка по краям и заливка левой половины.
  oledRect(0, 56, OLED_W, 8, true);
  oledFillRect(0, 56, OLED_W / 2, 8, true);

  oledFlush();
  Serial.println(F("test pattern drawn"));
}

void loop() {
  // Строка 5: счётчик. Должен расти раз в 500 мс.
  static char buf[16];
  oledText(0, 56, "TEST           ", 1);
  oledFmt(buf, (float)(tick % 1000), 3);
  oledText(64, 56, buf, 2);
  oledFlush();

  // Команды монитора: переключение контроллера и перерисовка узора.
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '5' || c == '6' || c == 'h') {
      OledController m = c == '5' ? OLED_SSD1315
                       : c == '6' ? OLED_SSD1306
                                  : OLED_SH1106;
      oledSetController(m);
      Serial.println(c == '5' ? F("mode ssd1315") : c == '6' ? F("mode ssd1306")
                                                            : F("mode sh1106"));
    }
  }
  tick++;
  delay(500);
}
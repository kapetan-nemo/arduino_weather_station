// Сканер I2C: перебирает все 128 адресов и печатает те, которые подтвердили
// транзакцию. Нужен для проверки проводки без мультиметра.
//
// Сканер ничего не пишет в регистры — только слушает, поэтому безопасно
// запускать на собранной схеме. Работает непрерывно, с паузой 3 секунды:
// чтобы увидеть изменение, достаточно отключить или посадить провод —
// перезаливка не нужна, достаточно нажать RESET на Arduino.
//
// Ожидаемый результат при правильной проводке: три адреса — 0x38 (AHT20),
// 0x77 (BMP280), 0x3C (дисплей, если резистор запаян в сторону 0x78).
//
// Если не ответил никто — нарушены SDA/SCL, GND или питание. Если отвечает
// только часть модулей — виноват один из них, отключай по одному.

#include <Wire.h>

void scan() {
  uint8_t found = 0;
  for (uint8_t addr = 8; addr < 120; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      found++;
      Serial.print(F("addr 0x"));
      if (addr < 0x10) Serial.print('0');
      Serial.print(addr, HEX);
      if (addr == 0x38) Serial.print(F("  AHT20"));
      if (addr == 0x77) Serial.print(F("  BMP280"));
      if (addr == 0x3C || addr == 0x3D) Serial.print(F("  OLED"));
      Serial.println();
      delay(20);
    }
  }
  Serial.print(F("found="));
  Serial.println(found);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
  Wire.begin();
  // Замедленная частота: на 100 кГц выше шанс разглядеть слабую связь,
  // в том числе если на SDA или SCL есть подтяжка вместо заземления.
  Wire.setClock(100000);
  Wire.setWireTimeout(20000, true);
  Serial.println(F("I2C SCAN, loop every 3s, press RESET to restart"));
}

void loop() {
  scan();
  delay(3000);
}
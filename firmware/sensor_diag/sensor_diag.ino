// Диагностика датчиков: печатает каждый шаг обмена отдельно, чтобы видно
// было, на каком байте обмен прерывается. Никакой записи в регистры, кроме
// обязательных команд сброса и запуска измерения.
//
// Что печатается:
//   1. ответ по адресу 0x38 (AHT20) — есть ли устройство на шине
//   2. байт состояния AHT20 до инициализации
//   3. soft reset 0xBA, затем повторное чтение состояния
//   4. инициализация 0xBE, затем состояние
//   5. запуск измерения 0xAC, затем 7 байт ответа
//   6. ответ по адресу 0x77 (BMP280)
//   7. chip ID из регистра 0xD0, ожидается 0x58
//
// Если пункт 1 не проходит — проблема в проводке или питании, дальше идти
// бессмысленно. Если 1 проходит, а 2 нет — обрыв на повторном START.

#include <Wire.h>

static const uint8_t AHT = 0x38;
static const uint8_t BMP = 0x77;

static void hex2(uint8_t v) {
  if (v < 0x10) Serial.print('0');
  Serial.print(v, HEX);
}

// Транзакция только с адресом: 0 — устройство подтвердило.
static uint8_t ping(uint8_t a) {
  Wire.beginTransmission(a);
  return Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
  Wire.begin();
  Wire.setClock(100000);
  Wire.setWireTimeout(20000, true);

  // 1. Наличие устройств.
  Serial.print(F("1 ping 0x38 = "));
  Serial.println(ping(AHT));
  delay(100);
  Serial.print(F("1 ping 0x77 = "));
  Serial.println(ping(BMP));
  delay(100);

  // 2. Состояние AHT20 до всякой инициализации.
  Serial.print(F("2 status before = "));
  if (ping(AHT) == 0) {
    Wire.beginTransmission(AHT);
    Wire.endTransmission(false);
    if (Wire.requestFrom(AHT, (uint8_t)1) == 1) {
      uint8_t st = Wire.read();
      hex2(st);
      Serial.println();
    } else {
      Serial.println(F("read failed"));
    }
  } else {
    Serial.println(F("no ack"));
  }
  delay(100);

  // 3. Soft reset.
  Wire.beginTransmission(AHT);
  Wire.write((uint8_t)0xBA);
  Serial.print(F("3 reset err = "));
  Serial.println(Wire.endTransmission());
  delay(40);
  Serial.print(F("3 status after reset = "));
  Wire.beginTransmission(AHT);
  if (Wire.endTransmission(false) == 0) {
    if (Wire.requestFrom(AHT, (uint8_t)1) == 1) {
      uint8_t st = Wire.read();
      hex2(st);
      Serial.println();
    } else {
      Serial.println(F("read failed"));
    }
  } else {
    Serial.println(F("no ack"));
  }
  delay(100);

  // 4. Инициализация 0xBE.
  Wire.beginTransmission(AHT);
  Wire.write((uint8_t)0xBE);
  Wire.write((uint8_t)0x08);
  Wire.write((uint8_t)0x00);
  Serial.print(F("4 init 0xBE err = "));
  Serial.println(Wire.endTransmission());
  delay(40);
  Serial.print(F("4 status after 0xBE = "));
  Wire.beginTransmission(AHT);
  if (Wire.endTransmission(false) == 0) {
    if (Wire.requestFrom(AHT, (uint8_t)1) == 1) {
      uint8_t st = Wire.read();
      hex2(st);
      Serial.println();
    } else {
      Serial.println(F("read failed"));
    }
  } else {
    Serial.println(F("no ack"));
  }
  delay(100);

  // 5. Запуск измерения.
  Wire.beginTransmission(AHT);
  Wire.write((uint8_t)0xAC);
  Wire.write((uint8_t)0x33);
  Wire.write((uint8_t)0x00);
  Serial.print(F("5 measure 0xAC err = "));
  Serial.println(Wire.endTransmission());
  delay(100);
  Serial.print(F("5 got "));
  uint8_t n = Wire.requestFrom(AHT, (uint8_t)7);
  Serial.print(n);
  Serial.print(F(" bytes:"));
  if (n == 7) {
    uint8_t d[7];
    for (uint8_t i = 0; i < 7; i++) d[i] = Wire.read();
    for (uint8_t i = 0; i < 7; i++) {
      Serial.print(' ');
      hex2(d[i]);
    }
    uint32_t h = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
    uint32_t t = (((uint32_t)d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];
    Serial.print(F("  -> T="));
    Serial.print((float)t * 200.0f / 1048576.0f - 50.0f, 2);
    Serial.print(F(" RH="));
    Serial.println((float)h * 100.0f / 1048576.0f, 2);
  } else {
    Serial.println();
  }
  delay(100);

  // 6. BMP280: chip ID.
  Wire.beginTransmission(BMP);
  Wire.write((uint8_t)0xD0);
  Serial.print(F("6 bmp readid err = "));
  Serial.println(Wire.endTransmission(false));
  if (Wire.requestFrom(BMP, (uint8_t)1) == 1) {
    uint8_t id = Wire.read();
    Serial.print(F("6 chip id = "));
    hex2(id);
    Serial.println(F("  (ожидается 58)"));
  } else {
    Serial.println(F("6 read failed"));
  }

  Serial.println(F("done"));
}

void loop() {}
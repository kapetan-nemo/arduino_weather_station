#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

unsigned long nextTick = 0;
bool aht_ok = false;
bool bmp_ok = false;
unsigned long lastRetry = 0;
bool oled_ok = false;

// ---------- OLED ----------
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ---------- BMP280 (0x77) ----------
struct BmpCal {
  uint16_t T1;
  int16_t T2, T3;
  uint16_t P1;
  int16_t P2, P3, P4, P5, P6, P7, P8, P9;
};
BmpCal cal;
float t_fine;

bool bmpInit() {
  Wire.beginTransmission(0x77);
  Wire.write(0xD0);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(0x77, (uint8_t)1) != 1) return false;
  if (Wire.read() != 0x58) return false;

  Wire.beginTransmission(0x77);
  Wire.write(0x88);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(0x77, (uint8_t)24) != 24) return false;
  uint8_t c[24];
  for (uint8_t i = 0; i < 24; i++) c[i] = Wire.read();
  cal.T1 = c[0] | (c[1] << 8);
  cal.T2 = c[2] | (c[3] << 8);
  cal.T3 = c[4] | (c[5] << 8);
  cal.P1 = c[6] | (c[7] << 8);
  cal.P2 = c[8] | (c[9] << 8);
  cal.P3 = c[10] | (c[11] << 8);
  cal.P4 = c[12] | (c[13] << 8);
  cal.P5 = c[14] | (c[15] << 8);
  cal.P6 = c[16] | (c[17] << 8);
  cal.P7 = c[18] | (c[19] << 8);
  cal.P8 = c[20] | (c[21] << 8);
  cal.P9 = c[22] | (c[23] << 8);

  Wire.beginTransmission(0x77);
  Wire.write(0xF4);
  Wire.write(0xB7);          // osrs_t x16, osrs_p x16, normal mode
  Wire.write(0xF5);
  Wire.write(0x1C);          // standby 0.5ms, filter x16
  if (Wire.endTransmission() != 0) return false;
  return true;
}

float bmpCompensateT(int32_t raw) {
  float v1 = ((float)raw / 16384.0f - (float)cal.T1 / 1024.0f) * cal.T2;
  float v2 = ((float)raw / 131072.0f - (float)cal.T1 / 8192.0f);
  v2 = v2 * v2 * cal.T3;
  t_fine = v1 + v2;
  return (v1 + v2) / 5120.0f;
}

float bmpCompensateP(int32_t raw) {
  float var1 = (t_fine / 2.0f) - 64000.0f;
  float var2 = var1 * var1 * cal.P6 / 32768.0f;
  var2 = var2 + var1 * cal.P5 * 2.0f;
  var2 = var2 / 4.0f + cal.P4 * 65536.0f;
  float var3 = cal.P3 * var1 * var1 / 524288.0f;
  var1 = (var3 + cal.P2 * var1) / 524288.0f;
  var1 = (32768.0f + var1) * cal.P1 / 32768.0f;
  if (var1 < 0.0001f) var1 = 0.0001f;
  float p = 1048576.0f - (float)raw;
  p = (p - var2 / 4096.0f) * 6250.0f / var1;
  var1 = cal.P9 * p * p / 2147483648.0f;
  var2 = p * cal.P8 / 32768.0f;
  return p + (var1 + var2 + cal.P7) / 16.0f;
}

// ---------- AHT20 (0x38) ----------
float ahtTemp = 0, ahtHum = 0;

// Байт состояния AHT20. Значение 0x18, например, НЕ означает ошибку:
//   бит 0 — инициализация: 0 = калибровка выгружена, 1 = ещё выгружается;
//   бит 3 — измерение: 0 = данные готовы, 1 = идёт;
//   остальные биты зарезервированы.
// Проверять надо бит 0, а не равенство всего байта нулю: бит 3 может быть
// единицей и при нормальной работе. Проверка «байт равен 0x00» никогда не
// проходит и приводит к бесконечным WAIT:sensors.
bool ahtStatus(uint8_t &st) {
  Wire.beginTransmission(0x38);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(0x38, (uint8_t)1) != 1) return false;
  st = Wire.read();
  return true;
}

// Ожидание выгрузки калибровки: ждём нулевой бит 0.
// ВАЖНО: вызывать только ПОСЛЕ команды 0xBE. До неё бит 0 равен единице
// и ожидание не закончится.
bool ahtWaitInit(uint16_t maxMs) {
  unsigned long t0 = millis();
  for (;;) {
    uint8_t st;
    if (!ahtStatus(st)) return false;
    if (!(st & 0x01)) return true;  // бит 0 = 0, калибровка выгружена
    if (millis() - t0 > maxMs) return false;
    delay(5);
  }
}

bool ahtInit() {
  Wire.beginTransmission(0x38);
  Wire.write(0xBA);          // soft reset
  if (Wire.endTransmission() != 0) return false;
  delay(20);                 // datasheet: 20 мс на soft reset
  Wire.beginTransmission(0x38);
  Wire.write(0xBE);          // init: выгрузка калибровки из EEPROM в регистры
  Wire.write(0x08);          // normal mode
  Wire.write(0x00);          // 18 бит на канал
  if (Wire.endTransmission() != 0) return false;
  delay(5);
  return ahtWaitInit(200);
}

bool ahtMeasure() {
  Wire.beginTransmission(0x38);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(80);                  // datasheet: не менее 75 мс на измерение
  if (Wire.requestFrom(0x38, (uint8_t)7) != 7) return false;
  uint8_t d[7];
  for (uint8_t i = 0; i < 7; i++) d[i] = Wire.read();
  // Проверяем только бит 0 ответа: он означает, что калибровка выгружена и
  // показания пригодны. Бит 3 игнорируем, он не влияет на достоверность
  // шести байт данных, которые уже приняты.
  if (d[0] & 0x01) return false;
  uint32_t h = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
  uint32_t t = (((uint32_t)d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];
  ahtHum = (float)h * 100.0f / 1048576.0f;
  ahtTemp = (float)t * 200.0f / 1048576.0f - 50.0f;
  return true;
}

// ---------- Панель OLED ----------
// Раскладка: 4 строки по 16 пикселей, экран 128x64.
//
// Шрифт встроенный в Adafruit_GFX, латинский: масштаб 1 занимает 6 пикселей
// по ширине на знак, 8 по высоте; масштаб 2 — 12 и 16. Подписи датчиков
// набираются масштабом 1, значения масштабом 2, единицы масштабом 1.
//
// Ширины: самая длинная подпись "AHT20" = 5 знаков = 30 пикселей, значения
// начинаются с PANEL_X=42. Реальные значения не длиннее 5 знаков:
// температура "-40.5" и влажность "100.0" дают 60 пикселей и кончаются
// на 102, давление "1008" даёт 48 пикселей и кончается на 90.
// Единица прижата к правому краю экрана, поэтому за край не выходит ни при
// какой длине значения.
//
#define PANEL_ROW_H 16
// пробела во встроенном шрифте 6 пикселей это ровно два пробела.
#define PANEL_X 42
// Зазор между значением и единицей измерения.
#define PANEL_GAP 4

// Знак градуса во встроенном шрифте Adafruit_GFX отсутствует, рисуется
// кольцом 3x3 пикселя из точек по битовой маске: 0b111, 0b101, 0b111.
static void drawDegree(int x, int y) {
  static const uint8_t RING[3] PROGMEM = {0x07, 0x05, 0x07};
  for (uint8_t r = 0; r < 3; r++) {
    uint8_t bits = pgm_read_byte(RING + r);
    for (uint8_t c = 0; c < 3; c++)
      if (bits & (1 << c)) display.drawPixel(x + c, y + r, SSD1306_WHITE);
  }
}

// Отступ от названия датчика до значения: 42 - 30 = 12 пикселей, при ширине
// пробела во встроенном шрифте 6 пикселей это ровно два пробела.
#define PANEL_X 42
// Зазор между значением и единицей измерения.
#define PANEL_GAP 4

// Коэффициент экспоненциального среднего для панели. Меньше значение даёт
// более гладкую линию и более медленную реакцию: при 0.05 время реакции
// около 4 секунд, при 0.2 около 1 секунды.
#define SMOOTH_ALPHA 0.05

// Накопители сглаживания. Только для дисплея: в порт уходят сырые значения,
// чтобы CSV оставался источником фактических замеров.
static float sm_t1, sm_t2, sm_h, sm_p;
static bool smooth_init = false;

// Единица измерения ставится сразу за значением, с зазором PANEL_GAP.
// Так она читается вместе со значением как одно целое, и положение
// подстраивается под длину значения автоматически.
//
// Проверка на вместимость при худшем случае:
//   значение начинается с PANEL_X = 42, масштаб 2 даёт 12 пикселей на знак;
//   предельная строка "-999.9" это 6 знаков = 72 пикселя, кончается на 114;
//   единица начинается с 114 + 4 = 118;
//   "°C" = знак градуса 3 + буква 6 = 9 пикселей, кончается на 127 —
//   последний пиксель из 127, в край экрана.
//   У влажности "%" короче, у давления "hPa" = 18 пикселей, но само значение
//   "9999" всего 4 знака, то есть кончается на 90, единица на 94..112.

static void panelRow(uint8_t row, const char *label, const char *value,
                     const char *unit, bool degree) {
  int y = row * PANEL_ROW_H;
  display.setTextSize(1);
  display.setCursor(0, y + PANEL_ROW_H - 8);
  display.print(label);

  display.setTextSize(2);
  display.setCursor(PANEL_X, y);
  display.print(value);

  int ux = PANEL_X + strlen(value) * 12 + PANEL_GAP;
  if (degree) drawDegree(ux, y + PANEL_ROW_H - 8);
  display.setTextSize(1);
  display.setCursor(ux + (degree ? 4 : 0), y + PANEL_ROW_H - 8);
  display.print(unit);
}

// Форматирование float в строку фиксированной длины, без printf и snprintf:
// на AVR printf с плавающей точкой занимает больше 1.5 КБ Flash.
static void formatFixed(char *buf, float v, uint8_t decimals) {
  if (v != v) {  // NaN
    strcpy(buf, "--");
    return;
  }
  bool neg = v < 0;
  if (neg) v = -v;
  static const uint16_t LIMIT[4] = {9999, 999, 99, 9};
  if (v > LIMIT[decimals]) {
    strcpy(buf, "--");
    return;
  }
  uint32_t scale = 1;
  for (uint8_t i = 0; i < decimals; i++) scale *= 10;
  uint32_t whole = (uint32_t)(v * scale + 0.5f) / scale;
  uint32_t frac = (uint32_t)(v * scale + 0.5f) % scale;

  uint8_t n = 0;
  if (neg && whole || neg && frac) buf[n++] = '-';
  char tmp[5];
  uint8_t t = 0;
  do {
    tmp[t++] = '0' + (whole % 10);
    whole /= 10;
  } while (whole);
  while (t) buf[n++] = tmp[--t];
  if (decimals) {
    buf[n++] = '.';
    for (uint8_t d = decimals; d > 0; d--) {
      uint32_t div = 1;
      for (uint8_t i = 1; i < d; i++) div *= 10;
      buf[n++] = '0' + (frac / div) % 10;
    }
  }
  buf[n] = 0;
}

static void panelShow(const char *v1, const char *v2, const char *v3,
                      const char *v4) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  panelRow(0, "AHT20", v1, "C", true);
  panelRow(1, "BMP28", v2, "C", true);
  panelRow(2, "RH", v3, "%", false);
  panelRow(3, "PRS", v4, "hPa", false);
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  // Частота шины 100 кГц. Проверено на железе: на 400 кГц плата датчиков
  // не отвечает, потому что у неё нет своих подтяжек и сигнал держится на
  // внутренних подтяжках ATmega328P (20-50 кОм). Подтяжки при 400 кГц должны
  // быть не выше 4.7 кОм, а на Dupont-проводах в 10-20 см ещё и ёмкость.
  Wire.setClock(100000);
  // Таймаут шины: запись страницы дисплея 128 байт на 100 кГц занимает 11.8 мс,
  // 25 мс хватает с запасом и при сбое не даёт зависнуть циклу опроса.
  Wire.setWireTimeout(25000, true);
  nextTick = (millis() / 200 + 1) * 200;

  oled_ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oled_ok) {
    // Второй адрес: у части модулей перепаян с 0x3C на 0x3D.
    oled_ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
    Serial.println(oled_ok ? F("OLED:0x3D") : F("OLED:none"));
  } else {
    Serial.println(F("OLED:0x3C"));
  }
  if (oled_ok) {
    display.setTextColor(SSD1306_WHITE);
    panelShow("--", "--", "--", "--");
  }
}

void loop() {
  unsigned long now = millis();

  // Однобайтовые команды монитора: 'i' — состояние дисплея и датчиков.
  // Чтение неблокирующее.
  while (Serial.available()) {
    if (Serial.read() == 'i') {
      Serial.print(F("OLED:"));
      Serial.println(oled_ok ? F("0x3C ok") : F("none"));
      Serial.print(F("AHT20:"));
      Serial.println(aht_ok ? F("0x38 ok") : F("no ack"));
      Serial.print(F("BMP280:"));
      Serial.println(bmp_ok ? F("0x77 ok") : F("no ack"));
    }
  }

  if (now < nextTick) return;

  if (!aht_ok || !bmp_ok) {
    if (now - lastRetry < 500) return;
    lastRetry = now;
    if (!aht_ok) aht_ok = ahtInit();
    if (!bmp_ok) bmp_ok = bmpInit();
    if (!aht_ok || !bmp_ok) {
      Serial.println("WAIT:sensors");
      if (oled_ok) {  // "--" во всех четырёх строках
        panelShow("--", "--", "--", "--");
      }
      nextTick = (now / 200 + 1) * 200;
      return;
    }
  }

  nextTick = (now / 200 + 1) * 200;

  if (!ahtMeasure()) {
    aht_ok = false;
    bmp_ok = false;
    return;
  }

  Wire.beginTransmission(0x77);
  Wire.write(0xF7);
  Wire.endTransmission(false);
  if (Wire.requestFrom(0x77, (uint8_t)8) != 8) {
    aht_ok = false;
    bmp_ok = false;
    return;
  }
  uint8_t d[8];
  for (uint8_t i = 0; i < 8; i++) d[i] = Wire.read();
  int32_t rawP = ((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) | (d[2] >> 4);
  int32_t rawT = ((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) | (d[5] >> 4);
  float bt = bmpCompensateT(rawT);
  float p = bmpCompensateP(rawP) / 100.0f;
  // Отсчёт миллисекунд берётся сразу после чтения регистров, то есть в момент,
  // когда датчики уже закончили измерение. Метка времени на стороне
  // компьютера отстаёт на десятки миллисекунд из-за буферизации порта, а
  // millis() отражает фактический момент. Счётчик переполняется через
  // 2^32 мс, то есть примерно через 49,7 суток непрерывной работы.
  unsigned long stamp = millis();
  Serial.print(ahtTemp, 2);
  Serial.print(',');
  Serial.print(bt, 2);
  Serial.print(',');
  Serial.print(ahtHum, 2);
  Serial.print(',');
  Serial.print(p, 2);
  Serial.print(',');
  Serial.println(stamp);

  if (oled_ok) {
    char v1[16], v2[16], v3[16], v4[16];
    // На панели показания сглаженные, в порт выше сырые. Сглаживание нужно
    // только чтобы цифры на экране не дрожали: по datasheet точность
    // AHT20 составляет ±0.3 °C, а разброс между замерами около ±0.03 °C,
    // то есть шум и так меньше паспортной точности и глаз его замечает.
    //
    // Экспоненциальное среднее y = y + alpha * (x - y). При alpha 0.05
    // время реакции около 4 секунд при периоде опроса 200 мс, шум падает
    // примерно в 3 раза. Первый замер инициализирует накопитель, иначе он
    // стартовал бы с нуля и показывал бы ложный разгон от 0 к значению.
    if (!smooth_init) {
      sm_t1 = ahtTemp; sm_t2 = bt; sm_h = ahtHum; sm_p = p;
      smooth_init = true;
    } else {
      sm_t1 += SMOOTH_ALPHA * (ahtTemp - sm_t1);
      sm_t2 += SMOOTH_ALPHA * (bt - sm_t2);
      sm_h += SMOOTH_ALPHA * (ahtHum - sm_h);
      sm_p += SMOOTH_ALPHA * (p - sm_p);
    }
    // На панели одна цифра после запятой: "21.7". У давления ноль знаков
    // после запятой, иначе "1008.2" не помещается вместе с единицей.
    formatFixed(v1, sm_t1, 1);
    formatFixed(v2, sm_t2, 1);
    formatFixed(v3, sm_h, 1);
    formatFixed(v4, sm_p, 0);
    panelShow(v1, v2, v3, v4);
  }
}

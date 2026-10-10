// Драйвер OLED 128x64 по I2C: только SSD1306/SH1106, без внешних библиотек.
//
// Работает через кадровый буфер в SRAM и отправляет на дисплей только те
// страницы, которые реально изменились (страница = 8 горизонтальных строк).
//
// Адрес инициализации шины: вызывать после Wire.begin() и Wire.setClock().
// Таймаут шины при этом должен быть не меньше времени передачи страницы:
// 129 байт (0x40 + 128 пикселей) на 400 кГц занимает 2.9 мс.
#ifndef OLED_H
#define OLED_H

#include <Arduino.h>

#define OLED_W 128
#define OLED_H_px 64
#define OLED_PAGES (OLED_H_px / 8)

// Тип контроллера. Определить по I2C невозможно: у SSD1306, SSD1315 и SH1106
// нет регистра идентификации, и ни один из них не поддерживает чтение данных
// из своей GDDRAM. Тип задаётся при инициализации.
enum OledController { OLED_SSD1315 = 0, OLED_SSD1306 = 1, OLED_SH1106 = 2 };

// Возвращает найденный адрес (0x3C или 0x3D) или 0, если дисплей не ответил.
// Сканирование выполняется одной транзакцией без данных (адрес + STOP).
uint8_t oledProbe();

// Инициализация GDDRAM, контроллера и очистка буфера.
// Возвращает false, если дисплей не найден или не ответил на запись.
bool oledInit(uint8_t addr, OledController ctrl);

// Смена контроллера на лету: применяются новые команды и очистка экрана.
void oledSetController(OledController ctrl);
OledController oledController();
uint8_t oledAddress();
bool oledReady();

void oledClear();
void oledFlush();  // отправить изменённые страницы
uint8_t oledDirtyPages();  // число страниц, ждущих отправки

// Примитивы. oledPixel помечает страницу изменённой только при реальном
// изменении байта, поэтому повторная отрисовка тех же данных не шлёт ничего.
void oledPixel(int x, int y, bool on);
void oledLine(int x0, int y0, int x1, int y1, bool on);
void oledRect(int x, int y, int w, int h, bool on);
void oledFillRect(int x, int y, int w, int h, bool on = true);

// Текст шрифтом font6x8 (5x7 в ячейке 6x8). scale: 1 или 2.
// Поддерживается ASCII 0x20..0x7E и знак градуса 0xB0.
void oledText(int x, int y, const char *s, uint8_t scale);

// Текст с инверсией: ячейки символов заливаются, глифы рисуются выключенными.
void oledTextInv(int x, int y, const char *s, uint8_t scale);

// Ширина строки в пикселях при заданном масштабе (без завершающего зазора).
uint8_t oledTextWidth(const char *s, uint8_t scale);

// Форматирование float без printf: снимок в char[16], длина в возврате.
// decimals: 0..3. Значения вне ±999.99 отсекаются, "--" при выходе за пределы.
uint8_t oledFmt(char *buf, float v, uint8_t decimals);

#endif  // OLED_H
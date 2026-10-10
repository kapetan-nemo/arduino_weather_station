#!/usr/bin/env python3
"""Предпросмотр раскладки панели OLED без железа.

Читает таблицу font6x8.h (её же использует прошивка) и повторяет логику
oledText / oledTextInv / panelRow в Python. Цель — проверить ширины строк
и отсутствие наложений до прошивки на плату.

Запуск:  .venv/bin/python tools/panel_preview.py
Результат: tools/panel_preview.png
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
HEADER = os.path.join(ROOT, "firmware", "bmp280_logger", "font6x8.h")
OUT = os.path.join(HERE, "panel_preview.png")

# Константы из bmp280_logger.ino и oled.h — должны совпадать.
PANEL_ROW_H = 16
PANEL_X = 32
PANEL_UNIT_X = 110
FONT6X8_FIRST = 32
DEG_INDEX = 95
W, H = 128, 64


def load_font():
    """Разбирает массив font6x8[N] из сгенерированного заголовка."""
    src = open(HEADER, encoding='utf-8').read()
    body = re.search(r'font6x8\[\d+\]\s*PROGMEM\s*=\s*\{(.*?)\};', src, re.S)
    if not body:
        raise SystemExit('в font6x8.h не найден массив font6x8')
    # Комментарии в теле массива содержат свои 0x-числа — убрать их,
    # иначе в таблицу попадут номера ячеек.
    raw = re.sub(r'//[^\n]*', '', body.group(1))
    data = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', raw)]
    assert len(data) % 8 == 0, len(data)
    return [data[i:i + 8] for i in range(0, len(data), 8)]


FONT = load_font()


def glyph_index(ch):
    code = ord(ch)
    if code == 0xB0:
        return DEG_INDEX
    if FONT6X8_FIRST <= code < FONT6X8_FIRST + len(FONT):
        return code - FONT6X8_FIRST
    return ord('?') - FONT6X8_FIRST


class Canvas:
    """128x64, бит 0 байта страницы — верхний пиксель, как в прошивке."""

    def __init__(self):
        self.px = [[0] * W for _ in range(H)]

    def pixel(self, x, y, on):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = 1 if on else 0

    def fill_rect(self, x, y, w, h, on=True):
        for j in range(h):
            for i in range(w):
                self.pixel(x + i, y + j, on)


def draw_text(cv, x, y, s, scale, on=True):
    for ch in s:
        cell = FONT[glyph_index(ch)]
        for r in range(7):
            bits = cell[r]
            for c in range(5):
                if not bits & (1 << c):
                    continue
                for dy in range(scale):
                    for dx in range(scale):
                        cv.pixel(x + c * scale + dx, y + r * scale + dy, on)
        x += 6 * scale
    return x


def text_width(s, scale):
    return len(s) * 6 * scale - scale if s else 0


def text_inv(cv, x, y, s, scale):
    n = text_width(s, scale)
    cv.fill_rect(x, y, n + scale, 8 * scale, True)
    draw_text(cv, x, y, s, scale, False)


def panel_row(cv, row, label, value, unit):
    y = row * PANEL_ROW_H
    text_inv(cv, 0, y + PANEL_ROW_H - 8, label, 1)
    draw_text(cv, PANEL_X, y + 1, value, 2)
    draw_text(cv, PANEL_UNIT_X, y + PANEL_ROW_H - 8, unit, 1)


def panel_row_plain(cv, row, label, value, unit):
    """Вариант без инверсии: плашка рисуется светлым текстом по тёмному фону."""
    y = row * PANEL_ROW_H
    draw_text(cv, 0, y + PANEL_ROW_H - 8, label, 1, True)
    draw_text(cv, PANEL_X, y + 1, value, 2)
    draw_text(cv, PANEL_UNIT_X, y + PANEL_ROW_H - 8, unit, 1)


def panel_show(cv, rows, plain=False):
    labels = ("AHT20", "BMP28", "RH", "PRS")
    units = ("°C", "°C", "%", "HPA")
    for i, value in enumerate(rows):
        if plain:
            panel_row_plain(cv, i, labels[i], value, units[i])
        else:
            panel_row(cv, i, labels[i], value, units[i])


def check(rows):
    """Проверяет, что ни одна строка не выходит за 128 пикселей."""
    labels = ("AHT20", "BMP28", "RH", "PRS")
    units = ("°C", "°C", "%", "HPA")
    ok = True
    for label, value, unit in zip(labels, rows, units):
        w_plate = text_width(label, 1) + 1
        w_value = text_width(value, 2)
        end_value = PANEL_X + w_value
        end_unit = PANEL_UNIT_X + text_width(unit, 1)
        problems = []
        if w_plate > PANEL_X:
            problems.append(f"плашка {w_plate}px наезжает на значение")
        if end_value > PANEL_UNIT_X:
            problems.append(f"значение кончается на {end_value}px")
        if end_unit > W:
            problems.append(f"единица кончается на {end_unit}px")
        flag = 'ok' if not problems else '; '.join(problems)
        print(f'{label:>6} {value:>8} {unit:>4}  '
              f'плашка={w_plate:>2} значение={end_value:>3} единица={end_unit:>3}  {flag}')
        ok &= not problems
    return ok


def render(rows):
    """Два варианта раскладки рядом: инвертированная плашка и обычный текст."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        print('matplotlib не установлен, PNG пропущен')
        return None

    fig, axes = plt.subplots(2, 1, figsize=(7.2, 7.6))
    for ax, plain, title in zip(
            axes,
            (False, True),
            ('плашка инвертированная (oledTextInv)',
             'плашка обычным текстом (oledText)')):
        cv = Canvas()
        panel_show(cv, rows, plain=plain)
        img = [[cv.px[y][x] for x in range(W)] for y in range(H)]
        ax.imshow(img, cmap='gray', vmin=0, vmax=1, interpolation='nearest')
        ax.set_xticks(range(0, W + 1, 8))
        ax.set_yticks(range(0, H + 1, 8))
        ax.tick_params(labelsize=6)
        ax.set_title(title, fontsize=8)
    axes[0].set_yticklabels([])
    fig.tight_layout()
    fig.savefig(OUT, dpi=170)
    plt.close(fig)
    return OUT


def main():
    cases = [
        ('обычные значения', ('21.7', '22.1', '48.3', '1008')),
        ('отрицательные температуры', ('-12.3', '-8.7', '55.0', '1021')),
        ('двузначное давление', ('24.0', '24.3', '60.1', '99')),
        ('нули', ('0.0', '0.0', '0.0', '960')),
        ('ошибка датчика', ('--', '--', '--', '--')),
    ]
    all_ok = True
    for name, rows in cases:
        print(f'\n== {name}')
        all_ok &= check(rows)
    out = render(('21.7', '22.1', '48.3', '1008'))
    print(f'\n{out}' if out else '')
    print('раскладка: все строки помещаются' if all_ok else 'РАСКЛАДКА: есть выход за край')
    return 0 if all_ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
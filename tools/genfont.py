#!/usr/bin/env python3
"""Генератор таблицы шрифта 5x7 для OLED (SSD1306 / SH1106).

Источник истины — ASCII-арт глифов в словаре GLYPHS. Из него скрипт собирает
заголовок firmware/bmp280_logger/font6x8.h: 96 ячеек по 8 байт (5 пикселей в
ширину + 1 пиксель межзнакового интервала, 7 строк глифа + 1 строка
интерлиньяжа), всего 768 байт в PROGMEM.

Ячейка упаковывается построчно: байт с номером y — строка глифа, бит x байта —
пиксель в столбце x. Бит 5 и выше не используются.

Дополнительно рисуется контрольный лист tools/font_specimen.png — по нему
проверяется начертание всех символов разом.

Запуск:  make font   (или  .venv/bin/python tools/genfont.py)
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
HEADER = os.path.join(ROOT, "firmware", "bmp280_logger", "font6x8.h")
SPECIMEN = os.path.join(HERE, "font_specimen.png")

GLYPH_W, GLYPH_H = 5, 7
CELL_W, CELL_H = 6, 8
ASCII_FIRST, ASCII_LAST = 32, 126
DEG_CODE = 0xB0  # знак градуса, индекс 96 в таблице
FALLBACK = "?"

# Каждый глиф: 7 строк по 5 символов, '#' — пиксель, '.' — фон, '/' — перевод строки.
GLYPHS = {
    ' ': '...../...../...../...../...../...../.....',
    '!': '..#../..#../..#../..#../..#../...../..#..',
    '%': '##.../##..#/...#./..#../.#.../#..##/...##',
    '(': '..##./.#.../#..../#..../#..../.#.../..##.',
    ')': '.##../...#./....#/....#/....#/...#./.##..',
    '+': '...../..#../..#../#####/..#../..#../.....',
    ',': '...../...../...../...../.##../.##../.#...',
    '-': '...../...../...../#####/...../...../.....',
    '.': '...../...../...../...../...../.##../.##..',
    '/': '....#/....#/...#./..#../.#.../#..../#....',
    '0': '.###./#...#/#..##/#.#.#/##..#/#...#/.###.',
    '1': '..#../.##../..#../..#../..#../..#../.###.',
    '2': '.###./#...#/....#/...#./..#../.#.../#####',
    '3': '#####/...#./..#../...#./....#/#...#/.###.',
    '4': '...#./..##./.#.#./#..#./#####/...#./...#.',
    '5': '#####/#..../####./....#/....#/#...#/.###.',
    '6': '..##./.#.../#..../####./#...#/#...#/.###.',
    '7': '#####/....#/...#./..#../.#.../.#.../.#...',
    '8': '.###./#...#/#...#/.###./#...#/#...#/.###.',
    '9': '.###./#...#/#...#/.####/....#/...#./.##..',
    ':': '...../.##../.##../...../.##../.##../.....',
    '=': '...../...../#####/...../#####/...../.....',
    '?': '.###./#...#/....#/...#./..#../...../..#..',
    'A': '.###./#...#/#...#/#####/#...#/#...#/#...#',
    'B': '####./#...#/#...#/####./#...#/#...#/####.',
    'C': '.####/#..../#..../#..../#..../#..../.####',
    'D': '####./#...#/#...#/#...#/#...#/#...#/####.',
    'E': '#####/#..../#..../####./#..../#..../#####',
    'F': '#####/#..../#..../####./#..../#..../#....',
    'G': '.###./#...#/#..../#.###/#...#/#...#/.###.',
    'H': '#...#/#...#/#...#/#####/#...#/#...#/#...#',
    'I': '.###./..#../..#../..#../..#../..#../.###.',
    'J': '..###/...#./...#./...#./...#./#..#./.##..',
    'K': '#...#/#..#./#.#../##.../#.#../#..#./#...#',
    'L': '#..../#..../#..../#..../#..../#..../#####',
    'M': '#...#/##.##/#.#.#/#.#.#/#...#/#...#/#...#',
    'N': '#...#/##..#/#.#.#/#..##/#...#/#...#/#...#',
    'O': '.###./#...#/#...#/#...#/#...#/#...#/.###.',
    'P': '####./#...#/#...#/####./#..../#..../#....',
    'Q': '.###./#...#/#...#/#...#/#.#.#/#..#./.##.#',
    'R': '####./#...#/#...#/####./#.#../#..#./#...#',
    'S': '.####/#..../#..../.###./....#/....#/####.',
    'T': '#####/..#../..#../..#../..#../..#../..#..',
    'U': '#...#/#...#/#...#/#...#/#...#/#...#/.###.',
    'V': '#...#/#...#/#...#/#...#/#...#/.#.#./..#..',
    'W': '#...#/#...#/#...#/#.#.#/#.#.#/##.##/#...#',
    'X': '#...#/#...#/.#.#./..#../.#.#./#...#/#...#',
    'Y': '#...#/#...#/.#.#./..#../..#../..#../..#..',
    'Z': '#####/....#/...#./..#../.#.../#..../#####',
    '[': '.###./.#.../.#.../.#.../.#.../.#.../.###.',
    ']': '.###./...#./...#./...#./...#./...#./.###.',
    '\xb0': '.##../#..#./.##../...../...../...../.....',
}


def parse_glyph(spec, name):
    """Превращает строку вида '##.../....' в 7 списков по 5 булевых значений."""
    rows = spec.split('/')
    if len(rows) != GLYPH_H:
        raise SystemExit(f"глиф {name!r}: {len(rows)} строк вместо {GLYPH_H}")
    out = []
    for r in rows:
        if len(r) != GLYPH_W:
            raise SystemExit(f"глиф {name!r}: строка {r!r} имеет ширину {len(r)} вместо {GLYPH_W}")
        out.append([c == '#' for c in r])
    return out


def build_table():
    """Возвращает список из 97 упакованных ячеек (ASCII 32..126 плюс градус)."""
    parsed = {ord(k): parse_glyph(v, k) for k, v in GLYPHS.items()}
    table = []
    for code in range(ASCII_FIRST, ASCII_LAST + 1):
        pix = parsed.get(code) or parsed[ord(FALLBACK)]
        table.append(pack(pix))
    table.append(pack(parsed[DEG_CODE]))
    return table


def pack(pix):
    """Матрица 7x5 -> 8 байт ячейки 6x8.

    Первые 7 байт — строки глифа (бит x — столбец x), восьмой байт —
    пустая строка интерлиньяжа, зарезервированная прошивкой.
    """
    cell = []
    for y in range(GLYPH_H):
        byte = 0
        for x in range(GLYPH_W):
            if pix[y][x]:
                byte |= 1 << x
        cell.append(byte)
    cell.append(0)
    return cell


def write_header(table):
    defined = sorted(
        chr(c) for c in range(ASCII_FIRST, ASCII_LAST + 1)
        if ord(chr(c)) in {ord(k) for k in GLYPHS}
    )
    lines = [
        '// Сгенерировано tools/genfont.py. Правьте GLYPHS там, не здесь.',
        '//',
        '// Таблица шрифта 5x7 в ячейках 6x8: 5 пикселей глифа + 1 пиксель',
        '// межзнакового интервала, 7 строк глифа + 1 строка интерлиньяжа.',
        '// Байт с номером y — строка глифа, бит x — пиксель в столбце x.',
        f'// Всего {len(table)} ячеек по {CELL_H} байт = {len(table) * CELL_H} байт в PROGMEM.',
        '//',
        f'// Индекс ячейки: код символа минус 32; знак градуса 0xB0 — индекс {len(table) - 1}.',
        '// Незаданные символы ASCII заменены на "' + FALLBACK + '".',
        '#ifndef FONT6X8_H',
        '#define FONT6X8_H',
        '',
        '#include <stdint.h>',
        '#include <avr/pgmspace.h>',
        '',
        f'#define FONT6X8_FIRST {ASCII_FIRST}',
        f'#define FONT6X8_COUNT {len(table)}',
        f'#define FONT6X8_DEG_INDEX {len(table) - 1}',
        f'#define FONT6X8_CELL_W {CELL_W}',
        f'#define FONT6X8_CELL_H {CELL_H}',
        '',
        f'static const uint8_t font6x8[{len(table) * CELL_H}] PROGMEM = {{',
    ]
    body = []
    for i, cell in enumerate(table):
        if i == ASCII_LAST - ASCII_FIRST + 1:
            body.append('  // 0xB0 — знак градуса')
        chunks = ', '.join(f'0x{b:02X}' for b in cell)
        body.append(f'  {chunks},  // {i:2d}')
    lines.append('\n'.join(body))
    lines += [
        '};',
        '',
        f'// Определены символы ({len(defined)}): {"".join(defined).replace(chr(0xb0), "°")}',
        '',
        '#endif  // FONT6X8_H',
        '',
    ]
    with open(HEADER, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    return HEADER


def write_specimen(table):
    """Контрольный лист: все символы сеткой 16x6, слева от строки — её начало."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        print('matplotlib не установлен, контрольный лист пропущен')
        return None

    cols, rows = 16, 6
    pad = 1
    img = [[1.0] * (cols * (CELL_W + pad)) for _ in range(rows * (CELL_H + pad))]
    for pos in range(ASCII_LAST - ASCII_FIRST + 1):
        idx = ASCII_FIRST + pos
        ci, ri = pos % cols, pos // cols
        cell = table[idx - ASCII_FIRST]
        for y in range(GLYPH_H):
            for x in range(GLYPH_W):
                if cell[y] & (1 << x):
                    img[ri * (CELL_H + pad) + y][ci * (CELL_W + pad) + x] = 0.0

    fig, ax = plt.subplots(figsize=(cols * 0.55, rows * 0.62))
    ax.imshow(img, cmap='gray_r', vmin=0, vmax=1, interpolation='nearest')
    ax.set_xticks([])
    ax.set_yticks([])
    for i in range(cols + 1):
        ax.axvline(i * (CELL_W + pad) - 0.5, color='0.8', lw=0.4)
    for i in range(rows + 1):
        ax.axhline(i * (CELL_H + pad) - 0.5, color='0.8', lw=0.4)
    for r in range(rows):
        code = ASCII_FIRST + r * cols
        ax.text(-1.6, r * (CELL_H + pad) + 3, f'0x{code:02X}', fontsize=5,
                va='center', ha='right', color='0.3')
    ax.set_title('font6x8: ASCII 0x20..0x7E, 5x7 в ячейке 6x8', fontsize=8)
    fig.tight_layout()
    fig.savefig(SPECIMEN, dpi=170)
    plt.close(fig)
    return SPECIMEN


def main():
    table = build_table()
    header = write_header(table)
    print(f'{header}: {len(table)} ячеек, {len(table) * CELL_H} байт')
    spec = write_specimen(table)
    if spec:
        print(f'{spec}: контрольный лист')


if __name__ == '__main__':
    sys.exit(main())
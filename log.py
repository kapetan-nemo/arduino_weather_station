#!/usr/bin/env python3
"""Запись показаний метеостанции в CSV.

Запуск:
    python log.py                      порт ищется автоматически
    python log.py /dev/cu.usbserial-X   порт указывается явно

Останавливается по Ctrl+C. Имя файла и время в CSV — местные, с указанием
часового пояса, чтобы записи разных дней сравнивались между собой.
"""
import csv
import sys
from datetime import datetime

import serial

from arduino_port import available, describe, find_port

BAUD = 115200

# В строке от Arduino пять полей: четыре показания и отсчёт миллисекунд.
# Отсчёт снимается в момент окончания измерения, метка времени компьютера
# ставится при чтении из порта и отстаёт на десятки миллисекунд из-за
# буферизации. Для точных интервалов используйте колонку uptime_ms.
HEADER = ["timestamp", "uptime_ms", "temp_aht20_c", "temp_bmp280_c",
          "humidity_pct", "pressure_hpa"]


def now_stamp():
    """Местное время с часовым поясом: 2026-10-10T14:34:02.123+03:00."""
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def show_ports(hint):
    print("Порты не найдены.")
    rows = available()
    if rows:
        for dev, desc, is_best in rows:
            mark = " ← будет выбран" if is_best else ""
            print(f"  {dev}  {desc}{mark}")
    print(f"\nУкажи порт явно: {hint} /dev/cu.usbserial-XXXX")


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    if not port:
        show_ports("python log.py")
        sys.exit(1)

    try:
        ser = serial.Serial(port, BAUD, timeout=2)
    except (OSError, serial.SerialException) as e:
        print(f"Не открывается порт {describe(port)}")
        print(f"Причина: {e}")
        show_ports("python log.py")
        sys.exit(1)

    ser.reset_input_buffer()
    filename = f"temperature_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    print(f"Порт {describe(port)}")
    print(f"Запись в {filename}, остановка по Ctrl+C")

    with open(filename, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(HEADER)
        f.flush()

        count = 0
        while True:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue
            if line.startswith("ERR:"):
                print(line)
                continue

            parts = line.split(",")
            if len(parts) != 5:
                continue

            temp_aht, temp_bmp, hum, pres, uptime = parts
            writer.writerow([now_stamp(), uptime, temp_aht, temp_bmp, hum, pres])
            f.flush()
            count += 1
            if count % 50 == 0:
                print(f"[{count}] {temp_aht}°C (bmp: {temp_bmp}°C)  "
                      f"{hum}%RH  {pres} hPa  t={uptime} мс")

    ser.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nОстановлено.")
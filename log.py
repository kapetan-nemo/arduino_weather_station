#!/usr/bin/env python3
import serial
import csv
import sys
import os
from datetime import datetime, timezone

BAUD = 115200


def find_port():
    for p in [
        "/dev/tty.usbserial-*",
        "/dev/tty.wchusbserial*",
        "/dev/tty.usbmodem*",
    ]:
        import glob
        matches = glob.glob(p)
        if matches:
            return matches[0]
    return None


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    if not port:
        print("Cannot find Arduino port. Pass it as arg: python log.py /dev/tty.usbserial-1420")
        sys.exit(1)

    filename = f"temperature_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    print(f"Opening {port}, writing to {filename}")

    ser = serial.Serial(port, BAUD, timeout=2)
    ser.reset_input_buffer()

    with open(filename, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp", "temp_aht20_c", "temp_bmp280_c", "humidity_pct", "pressure_hpa"])
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
            if len(parts) != 4:
                continue

            temp_aht, temp_bmp, hum, pres = parts
            ts = datetime.now(timezone.utc).isoformat()
            writer.writerow([ts, temp_aht, temp_bmp, hum, pres])
            f.flush()
            count += 1
            if count % 10 == 0:
                print(f"[{count}] {temp_aht}°C (bmp: {temp_bmp}°C)  {hum}%RH  {pres} hPa")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped.")

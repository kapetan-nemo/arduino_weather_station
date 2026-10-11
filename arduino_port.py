#!/usr/bin/env python3
"""Поиск последовательного порта Arduino. Работает на macOS, Linux и Windows.

Вместо жёстко прописанных шаблонов вида /dev/tty.usbserial-* используется
list_ports из pyserial: он спрашивает у операционной системы список устройств
и отдаёт готовые имена. На macOS это /dev/cu.*, на Linux /dev/ttyACM* или
/dev/ttyUSB*, на Windows COM3 и так далее. Один и тот же код работает везде.

Отдельная тонкость macOS: один физический порт виден в системе дважды, как
/dev/cu.X и как /dev/tty.X. Имена с префиксом tty работают в блокирующем
режиме и вешают скрипт намертво, поэтому выбирается обязательно cu.
"""
from serial.tools import list_ports

# Идентификаторы USB-микросхем, которые ставят в Arduino и его клоны.
KNOWN_VID = {
    0x1A86: "CH340",
    0x0403: "FTDI",
    0x10C4: "CP210x",
    0x2341: "Arduino",
    0x2A03: "Arduino",
}

# Подстроки в описании устройства, по которым порт узнаётся без VID.
KNOWN_TEXT = ("usb", "serial", "uart", "arduino", "ch340", "ftdi", "cp210")


def _rank(port):
    """Меньше значение — лучше порт."""
    score = 0
    if port.vid in KNOWN_VID:
        score -= 100
    elif port.vid is None:
        # Устройство без USB-идентификатора: системные псевдопорты вроде
        # cu.debug-console и Bluetooth. Настоящей Arduino так не бывает.
        score += 60
    desc = (port.description or "").lower()
    if any(t in desc for t in KNOWN_TEXT):
        score -= 10
    if desc in ("n/a", ""):
        score += 30
    if port.device.startswith("/dev/tty."):
        # Блокирующий вариант на macOS: читать из него нельзя.
        score += 50
    return score


def find_port():
    """Возвращает имя порта или None, если подходящих устройств нет."""
    ports = list(list_ports.comports())
    if not ports:
        return None
    return sorted(ports, key=_rank)[0].device


def describe(port):
    """Человекочитаемое описание порта для сообщений об ошибке."""
    for p in list_ports.comports():
        if p.device == port:
            vid = f"{p.vid:04X}" if p.vid is not None else "????"
            pid = f"{p.pid:04X}" if p.pid is not None else "????"
            chip = KNOWN_VID.get(p.vid, "")
            return f"{port} — {p.description} (VID:PID {vid}:{pid}{', ' + chip if chip else ''})"
    return port


def available():
    """Список портов с пометкой, какой из них будет выбран."""
    ports = list(list_ports.comports())
    if not ports:
        return []
    best = sorted(ports, key=_rank)[0].device
    return [(p.device, p.description or "?", p.device == best) for p in ports]


def main():
    """Вызывается как python arduino_port.py — показывает найденные порты."""
    rows = available()
    if not rows:
        print("Порты не найдены. Проверь, что Arduino подключена к USB.")
        return 1
    for dev, desc, is_best in rows:
        mark = "  <- будет выбран" if is_best else ""
        print(f"  {dev}  {desc}{mark}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
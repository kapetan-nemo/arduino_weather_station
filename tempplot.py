#!/usr/bin/env python3
"""Реалтайм-график 60 fps: слева температура (AHT20+BMP280), справа давление.
Ховер: ближайшая линия → значение, время, источник. CSV пишется параллельно (1 Гц)."""
import argparse
import collections
import csv
import glob
import sys
import time
from datetime import datetime, timezone

import matplotlib.pyplot as plt
import matplotlib.animation as animation
import serial

BAUD = 115200
DATA_HZ = 5
CSV_EVERY = 5        # строка в CSV раз в 5 отсчётов = 1 Гц
WINDOW_SEC = 300     # 5 минут истории
LIVE_HZ = 60

SOURCES = [("AHT20", 1, "left"), ("BMP280", 2, "left"), ("Давление", 4, "right")]


def find_port():
    for pat in ("/dev/tty.usbserial-*", "/dev/tty.wchusbserial*", "/dev/tty.usbmodem*"):
        m = glob.glob(pat)
        if m:
            return m[0]
    return None


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    if not port:
        print("Port not found")
        sys.exit(1)

    filename = f"temperature_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    print(f"Logging to {filename}")
    csv_file = open(filename, "w", newline="")
    writer = csv.writer(csv_file)
    writer.writerow(["timestamp", "temp_aht20_c", "temp_bmp280_c", "humidity_pct", "pressure_hpa"])
    csv_file.flush()
    csv_count = 0

    ser = serial.Serial(port, BAUD, timeout=0.05)
    ser.reset_input_buffer()

    samples = collections.deque(maxlen=WINDOW_SEC * DATA_HZ)  # (t, ta, tb, h, p)
    last_sample = time.time()
    last_attempt = 0.0

    def reconnect():
        nonlocal ser, last_attempt
        last_attempt = time.time()
        try:
            ser.close()
        except Exception:
            pass
        ser = None
        p = find_port()
        if not p:
            return
        try:
            ser = serial.Serial(p, BAUD, timeout=0.05)
            ser.reset_input_buffer()
            print(f"reconnected: {p}", flush=True)
        except Exception:
            ser = None

    fig, ax_l = plt.subplots(figsize=(10, 6))
    ax_r = ax_l.twinx()
    (l1,) = ax_l.plot([], [], color="#00b300", lw=1.2, label="AHT20")
    (l2,) = ax_l.plot([], [], color="#0066ff", lw=1.2, label="BMP280")
    (l3,) = ax_r.plot([], [], color="#ff6600", lw=1.2, label="Давление")
    ax_l.set_ylabel("Температура, °C", color="#00b300")
    ax_r.set_ylabel("Давление, гПа", color="#ff6600")
    ax_l.tick_params(axis="y", labelcolor="#00b300")
    ax_r.tick_params(axis="y", labelcolor="#ff6600")
    ax_l.grid(True, color="#dddddd", lw=0.7)
    ax_r.grid(False)
    ax_l.set_xlim(-WINDOW_SEC, 0)
    live_text = ax_l.text(
        0.985, 0.98, "", transform=ax_l.transAxes, ha="right", va="top",
        fontsize=10, family="monospace",
        bbox=dict(boxstyle="round", facecolor="white", edgecolor="#999999", alpha=0.85),
    )
    ax_l.legend(handles=[l1, l2, l3], loc="upper left")
    hover = ax_l.annotate(
        "", xy=(0, 0), xytext=(12, 12), textcoords="offset points",
        fontsize=9, family="monospace", ha="left", va="bottom",
        bbox=dict(boxstyle="round", facecolor="#ffffcc", edgecolor="#aaaa00", alpha=0.9),
        arrowprops=dict(arrowstyle="->", color="#aaaa00"),
    )
    hover.set_visible(False)
    hover_r = ax_r.annotate(
        "", xy=(0, 0), xytext=(12, 12), textcoords="offset points",
        fontsize=9, family="monospace", ha="left", va="bottom",
        bbox=dict(boxstyle="round", facecolor="#ffffcc", edgecolor="#aaaa00", alpha=0.9),
        arrowprops=dict(arrowstyle="->", color="#aaaa00"),
    )
    hover_r.set_visible(False)
    (snap_l,) = ax_l.plot([], [], "o", color="#aaaa00", ms=6)
    (snap_r,) = ax_r.plot([], [], "o", color="#aaaa00", ms=6)
    snap_l.set_visible(False)
    snap_r.set_visible(False)
    fig.tight_layout()

    def update(_):
        nonlocal csv_count, last_sample, ser
        now = time.time()
        if ser is None:
            if now - last_attempt > 1.0:
                reconnect()
        if ser is not None:
            try:
                raw = ser.read(max(ser.in_waiting, 1) + 1024)
            except Exception:
                ser = None
                raw = b""
            for line in raw.decode("utf-8", "replace").replace("\r", "").split("\n"):
                parts = line.strip().split(",")
                if len(parts) != 4:
                    continue
                try:
                    t_a, t_b, h, p = float(parts[0]), float(parts[1]), float(parts[2]), float(parts[3])
                except ValueError:
                    continue
                samples.append((time.time(), t_a, t_b, h, p))
                csv_count += 1
                last_sample = time.time()
                if csv_count % CSV_EVERY == 0:
                    writer.writerow([datetime.now(timezone.utc).isoformat(), t_a, t_b, h, p])
            csv_file.flush()
            if now - last_sample > 3.0 and now - last_attempt > 1.0:
                reconnect()

        if samples:
            ts = [s[0] for s in samples]
            xs = [t - now for t in ts]
            l1.set_data(xs, [s[1] for s in samples])
            l2.set_data(xs, [s[2] for s in samples])
            l3.set_data(xs, [s[4] for s in samples])
            all_t = [s[1] for s in samples] + [s[2] for s in samples]
            pad = 0.3
            ax_l.set_ylim(min(all_t) - pad, max(all_t) + pad)
            ax_r.set_ylim(min(s[4] for s in samples) - 0.3, max(s[4] for s in samples) + 0.3)
            live_text.set_text(
                f"AHT20:  {samples[-1][1]:.2f} °C\nBMP280: {samples[-1][2]:.2f} °C\n"
                f"Влажн.: {samples[-1][3]:.1f} %\nДавл.:  {samples[-1][4]:.2f} гПа"
            )

    def on_move(event):
        if event.inaxes is None or not samples:
            for a in (hover, hover_r, snap_l, snap_r):
                if a.get_visible():
                    a.set_visible(False)
            return
        x_cur, y_cur = ax_l.transData.inverted().transform((event.x, event.y))
        y_cur_r = ax_r.transData.inverted().transform((event.x, event.y))[1]
        now = time.time()
        t_target = now + x_cur
        t_arr = [s[0] for s in samples]
        best = None
        for name, idx, side in SOURCES:
            j = min(range(len(t_arr)), key=lambda i: abs(t_arr[i] - t_target))
            v = samples[j][idx]
            if side == "right":
                ylo, yhi = ax_r.get_ylim()
                yv = y_cur_r
            else:
                ylo, yhi = ax_l.get_ylim()
                yv = y_cur
            dy = abs(v - yv) / max(yhi - ylo, 1e-9)
            if best is None or dy < best[0]:
                best = (dy, name, idx, j, side)
        if best is None or best[0] > 0.35:
            for a in (hover, hover_r, snap_l, snap_r):
                a.set_visible(False)
            return
        _, name, idx, j, side = best
        t_s = samples[j][0]
        if name == "Давление":
            val, unit = samples[j][4], "гПа"
        else:
            val = samples[j][1] if name == "AHT20" else samples[j][2]
            unit = "°C"
        hh = datetime.fromtimestamp(t_s).strftime("%H:%M:%S")
        x = t_s - time.time()
        label = f"{name}: {val:.2f} {unit}\n{hh}"
        if side == "right":
            snap_l.set_visible(False)
            snap_r.set_data([x], [val])
            snap_r.set_visible(True)
        else:
            snap_r.set_visible(False)
            snap_l.set_data([x], [val])
            snap_l.set_visible(True)
        hover_r.set_visible(False)
        hover.xy = (x_cur, y_cur)
        hover.set_text(label)
        hover.set_visible(True)
        fig.canvas.draw_idle()

    ani = animation.FuncAnimation(fig, update, interval=1000 // LIVE_HZ,
                                  blit=False, cache_frame_data=False)
    fig.canvas.mpl_connect("motion_notify_event", on_move)
    plt.show()
    ser.close()
    csv_file.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)

#!/usr/bin/env python3
"""Мини-осциллограф в стиле ЭЛТ: чёрный фон, зелёный луч, сетка, подписи в вольтах."""
import argparse
import collections
import sys
import time

import matplotlib.pyplot as plt
import matplotlib.animation as animation
import serial

GREEN = "#00ff41"
GRID = (0, 1.0, 0.1, 0.22)
GRID_CENTER = (0, 1.0, 0.1, 0.5)
X_DIVS = 10
Y_DIVS = 5
VREF = 5.0
MAX_Y = 1023
TRIGGER_LEVEL = MAX_Y // 2


class Scope:
    def __init__(self, ser, window, auto_holdown):
        self.ser = ser
        self.buf = [0] * window
        self.window = window
        self.auto_holdown = auto_holdown
        self.prev = None
        self.beam_x = 0
        self.state = "wait"
        self.waited = 0
        self.rate = collections.deque(maxlen=20)
        self.t_start = time.time()

    def step(self, v):
        if self.prev is None:
            self.prev = v
            return
        if self.state == "wait":
            self.waited += 1
            if (self.prev < TRIGGER_LEVEL <= v) or self.waited >= self.auto_holdown:
                self.state = "sweep"
                self.beam_x = 0
                self.waited = 0
                self.buf[self.beam_x] = v
                self.beam_x += 1
        else:
            self.buf[self.beam_x] = v
            self.beam_x += 1
            if self.beam_x >= self.window:
                self.state = "wait"
                self.waited = 0
        self.prev = v


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("port", nargs="?", default="/dev/cu.usbserial-A5069RR4")
    parser.add_argument("--baud", type=int, default=1000000)
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=2)
    ser.reset_input_buffer()

    samples_per_div = 90
    window = samples_per_div * X_DIVS

    fig, ax = plt.subplots(figsize=(9, 6), facecolor="black")
    ax.set_facecolor("black")
    ax.set_ylim(0, MAX_Y)
    ax.set_xlim(0, X_DIVS)
    for x in range(X_DIVS + 1):
        ax.axvline(x, color=GRID, lw=1)
    for y in range(Y_DIVS + 1):
        lw = 1.4 if y in (0, Y_DIVS // 2, Y_DIVS) else 1.0
        ax.axhline(y * MAX_Y / Y_DIVS, color=GRID, lw=lw)
    ax.axhline(TRIGGER_LEVEL, color=(0.2, 0.4, 0.2), lw=0.6, ls=":")

    ax.set_xticks([i for i in range(X_DIVS + 1)])
    ax.set_xticklabels([f"{i * 10:.0f}" for i in range(X_DIVS + 1)], color=GREEN, fontsize=9)
    ax.set_yticks([i * MAX_Y / Y_DIVS for i in range(Y_DIVS + 1)])
    ax.set_yticklabels([f"{i:.0f}V" for i in range(Y_DIVS + 1)], color=GREEN, fontsize=9)
    for spine in ax.spines.values():
        spine.set_color(GRID_CENTER)
    title = ax.set_title("  SCOPE  ~? kS/s  10ms/div", color=GREEN, fontsize=11, loc="left")

    scope = Scope(ser, window=window, auto_holdown=window)
    (glow,) = ax.plot([], [], color=GREEN, lw=4, alpha=0.28)
    (trace,) = ax.plot([], [], color=GREEN, lw=1.2)
    (beam,) = ax.plot([], [], "o", color="#eaffea", ms=4)

    def update(_):
        raw = ser.read(max(ser.in_waiting, 1) + 4096)
        got = 0
        for line in raw.decode("utf-8", "replace").replace("\r", "").split("\n"):
            line = line.strip()
            if not line.isdigit():
                continue
            scope.step(min(MAX_Y, int(line)))
            got += 1
        if got:
            elapsed = time.time() - scope.t_start
            if elapsed > 0.5:
                rate = len(scope.rate) and sum(scope.rate) / len(scope.rate)
                scope.rate.append(got / max(elapsed - (scope.rate[0] if scope.rate else 0), 1e-9))
                if rate:
                    title.set_text(f"  SCOPE  {rate / 1000:.1f} kS/s  10 ms/div")
                scope.t_start = time.time()
                scope.rate.clear()
            n = len(scope.buf)
            xs = [i / samples_per_div for i in range(n)]
            trace.set_data(xs, scope.buf)
            glow.set_data(xs, scope.buf)
            beam.set_data([scope.beam_x / samples_per_div], [scope.buf[min(scope.beam_x, n - 1)]])
        return trace, glow, beam

    ani = animation.FuncAnimation(fig, update, interval=20, blit=True, cache_frame_data=False)
    plt.show()
    ser.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)

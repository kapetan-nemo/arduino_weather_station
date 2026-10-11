PORT ?=
FQBN := arduino:avr:nano:cpu=atmega328
PY := .venv/bin/python
SKETCH := firmware/bmp280_logger

# PORT передаётся скриптам только если он задан, иначе они ищут порт сами.
# На macOS порт выглядит как /dev/cu.usbserial-A5069RR4, на Linux как
# /dev/ttyACM0, на Windows как COM3.
PORTARG := $(if $(PORT),$(PORT),)

.DEFAULT_GOAL := run

.PHONY: run log scope ports flash compile venv clean help

help:
	@echo "make            — график в реальном времени и запись CSV (1 Гц)"
	@echo "make log        — только запись CSV (5 Гц)"
	@echo "make scope      — осциллограф"
	@echo "make ports      — показать доступные порты"
	@echo "make flash      — залить прошивку, порт обязателен"
	@echo "make compile    — скомпилировать прошивку"
	@echo "make clean      — удалить CSV-файлы"
	@echo ""
	@echo "Порт можно задать явно: make log PORT=/dev/cu.usbserial-A5069RR4"
	@echo "Без PORT скрипты ищут Arduino сами."

run: | venv
	$(PY) tempplot.py $(PORTARG)

log: | venv
	$(PY) log.py $(PORTARG)

scope: | venv
	$(PY) scope.py $(PORTARG)

ports: | venv
	$(PY) arduino_port.py

flash:
ifndef PORT
	$(error порт не задан. Использовать: make flash PORT=/dev/cu.usbserial-A5069RR4)
endif
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) $(SKETCH)

compile:
	arduino-cli compile --fqbn $(FQBN) $(SKETCH)

venv:
	@test -x $(PY) || { \
		python3 -m venv .venv && \
		.venv/bin/pip install --quiet pyserial matplotlib; \
	}

clean:
	rm -f temperature_*.csv
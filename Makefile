PORT := /dev/cu.usbserial-A5069RR4
FQBN := arduino:avr:nano:cpu=atmega328
PY := .venv/bin/python
SKETCH := firmware/bmp280_logger

.DEFAULT_GOAL := run

.PHONY: run log scope flash compile venv clean help

help:
	@echo "make          — график + запись CSV"
	@echo "make log      — только CSV"
	@echo "make scope    — осциллограф"
	@echo "make flash    — залить прошивку"
	@echo "make compile  — скомпилировать прошивку"
	@echo "make clean    — удалить CSV-файлы"

run: | venv
	$(PY) tempplot.py

log: | venv
	$(PY) log.py

scope: | venv
	$(PY) scope.py

flash:
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

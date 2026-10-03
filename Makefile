ARDUINO_CLI ?= /Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli
CLI := "$(ARDUINO_CLI)"
ESP32_INDEX := https://espressif.github.io/arduino-esp32/package_esp32_index.json
FQBN := esp32:esp32:XIAO_ESP32C5:CDCOnBoot=cdc
SKETCH := btc_alerts
BUILD_DIR := build
PORT ?= $(firstword $(wildcard /dev/cu.usbmodem*))
ARDUINO_LIBS ?= $(HOME)/Documents/Arduino/libraries

CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -I$(SKETCH) -I$(ARDUINO_LIBS)/ArduinoJson/src

# Módulos puros (sin Arduino.h) que se compilan y prueban en el host.
PURE_SRC := $(SKETCH)/alert_logic.cpp $(SKETCH)/decimal.cpp $(SKETCH)/command_parser.cpp \
            $(SKETCH)/price_parser.cpp $(SKETCH)/price_health.cpp $(SKETCH)/messages.cpp
TESTS := alert_logic decimal command_parser price_parser price_health messages

.PHONY: setup test compile flash monitor clean

setup:
	$(CLI) core update-index --additional-urls $(ESP32_INDEX)
	$(CLI) core install esp32:esp32 --additional-urls $(ESP32_INDEX)
	$(CLI) lib update-index
	$(CLI) lib install ArduinoJson UniversalTelegramBot

test: $(TESTS:%=test/bin/test_%)
	@for t in $^; do echo "== $$t"; ./$$t || exit 1; done

test/bin/test_%: test/test_%.cpp test/test.h $(PURE_SRC) $(wildcard $(SKETCH)/*.h)
	@mkdir -p test/bin
	$(CXX) $(CXXFLAGS) $< $(PURE_SRC) -o $@

compile:
	@test -f $(SKETCH)/secrets.h || { echo "Falta $(SKETCH)/secrets.h -> cp $(SKETCH)/secrets.example.h $(SKETCH)/secrets.h"; exit 1; }
	$(CLI) compile --fqbn $(FQBN) --build-path $(BUILD_DIR) $(SKETCH)

flash: compile
	@test -n "$(PORT)" || { echo "No encuentro el XIAO en /dev/cu.usbmodem*. Usa: make flash PORT=/dev/cu.xxx"; exit 1; }
	$(CLI) upload --fqbn $(FQBN) --port $(PORT) --input-dir $(BUILD_DIR) $(SKETCH)

monitor:
	@test -n "$(PORT)" || { echo "No encuentro el XIAO en /dev/cu.usbmodem*. Usa: make monitor PORT=/dev/cu.xxx"; exit 1; }
	$(CLI) monitor --port $(PORT) --config baudrate=115200

clean:
	rm -rf $(BUILD_DIR) test/bin

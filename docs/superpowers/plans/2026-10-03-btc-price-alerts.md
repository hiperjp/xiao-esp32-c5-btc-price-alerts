# BTC Price Alerts (XIAO ESP32-C5) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Firmware para el XIAO ESP32-C5 que vigila el precio BTC/USD y avisa por Telegram al cruzar un umbral inferior o superior configurable desde el propio chat.

**Architecture:** Sketch Arduino en `btc_alerts/`. La lógica con riesgo (disparo/rearme, parseo de comandos, parseo de precios, salud de la fuente, textos) vive en módulos C++ puros sin `Arduino.h`, probados en macOS con `clang++`. Los módulos de hardware/red (`settings_store`, `price_source`, `telegram_bot`) son glue fino sobre el core ESP32, verificados con `make compile` y en el dispositivo.

**Tech Stack:** C++17, Arduino core `esp32:esp32` ≥ 3.3 (FQBN `esp32:esp32:XIAO_ESP32C5`), ArduinoJson v7, UniversalTelegramBot 1.3.x, `arduino-cli` incluido en Arduino IDE 2.x, `make`, `clang++`.

**Spec:** `docs/superpowers/specs/2026-10-03-btc-price-alerts-design.md`

## Global Constraints

- Directorio del proyecto: `/Users/hjp/WS/esp32/xiao-esp32-c5-btc-price-alerts`. Todos los paths son relativos a él.
- `arduino-cli`: `/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli` (no instalar otro).
- FQBN: `esp32:esp32:XIAO_ESP32C5:CDCOnBoot=cdc`; core `esp32:esp32` ≥ 3.3.
- Librerías: `ArduinoJson` (v7, API `JsonDocument`) y `UniversalTelegramBot`.
- Los módulos puros (`alert_logic`, `decimal`, `command_parser`, `price_parser`, `price_health`, `messages`) **no** incluyen `Arduino.h` ni APIs del ESP32; usan `std::string` y `<cstdint>`.
- Tiempos de `millis()` se manejan como `uint32_t`/`unsigned long` con resta sin signo (`now - last >= interval`), nunca `now >= last + interval`.
- TLS siempre verificado con `useBuiltinCACertBundle()`; prohibido `setInsecure()`.
- `btc_alerts/secrets.h` nunca se versiona (ya está en `.gitignore`).
- Margen de rearme 0.005; precio cada 60 s; polling Telegram cada 3 s; aviso de fallo tras 10 min; timeout HTTP 10 s.
- Textos al usuario en español, con los formatos exactos de la spec: `🔻 BTC bajó de 90,000 USD → ahora 89,950 USD`, `🚀 BTC superó 130,000 USD → ahora 130,120 USD`, `🟢 Online — BTC = <precio> USD`, `⚠️ No puedo obtener el precio de BTC`, `✅ Precio recuperado: <precio> USD`.
- Commits sin trailer `Co-Authored-By`.

## Review Focus

1. **Separador de miles al escribir umbrales** (`/low 90.000`, `/low 90,000`): la coma se rechaza con un mensaje de uso; `90.000` es decimal válido (= 90), así que la confirmación debe mostrar `90 USD` para que el error sea evidente → tests en Task 2 (parser) y Task 4 (`thresholdSetMsg(true, 90)`).
2. **Fallo de Telegram justo al cruzar un umbral**: la alerta no debe perderse; el umbral queda armado y se reintenta en el siguiente ciclo → `commitAlerts` testeado en Task 1.
3. **Respuesta HTTP 200 con cuerpo inesperado** (bloqueo geográfico de Binance `{"code":0,"msg":...}`, precio `"0"`, HTML, JSON truncado): debe contar como fallo, nunca como precio 0 que dispare una alerta LOW → tests en Task 3.
4. **Variantes de escritura de comandos** (`/LOW 90000`, espacios extra, `/low@hjpBtcAlertsBot 90000`): deben funcionar igual que `/low 90000` → tests en Task 2.
5. **Desborde de `millis()` (~49.7 días)** al medir los 10 min sin precio: no debe disparar ni bloquear el aviso → test en Task 3 (`price_health`).

---

## File Structure

| Archivo | Responsabilidad |
|---|---|
| `Makefile` | `setup`, `test`, `compile`, `flash`, `monitor`, `clean` |
| `test/test.h` | Mini framework de asserts para tests en el host |
| `test/test_*.cpp` | Un ejecutable de test por módulo puro |
| `btc_alerts/btc_alerts.ino` | `setup()`/`loop()`: WiFi, NTP, timers, orquestación |
| `btc_alerts/config.h` | Constantes (intervalos, margen, URLs, NTP) |
| `btc_alerts/secrets.example.h` | Plantilla de credenciales |
| `btc_alerts/alert_logic.{h,cpp}` | Estado de umbrales, disparo/rearme, set con validación |
| `btc_alerts/decimal.{h,cpp}` | Parseo estricto de decimales positivos |
| `btc_alerts/command_parser.{h,cpp}` | Texto de Telegram → `Command` |
| `btc_alerts/price_parser.{h,cpp}` | JSON Binance/Coinbase → precio |
| `btc_alerts/price_health.{h,cpp}` | Decide cuándo avisar de fallos sostenidos |
| `btc_alerts/messages.{h,cpp}` | Todos los textos y formateo de números/duraciones |
| `btc_alerts/settings_store.{h,cpp}` | Persistencia de `AlertState` en NVS |
| `btc_alerts/price_source.{h,cpp}` | HTTPS GET con respaldo Binance → Coinbase |
| `btc_alerts/telegram_bot.{h,cpp}` | Envío, polling, filtro por `chat_id` |

Nota respecto de la spec: se añaden `decimal`, `price_health` y `messages` como módulos puros separados para poder testear en el host el parseo de números, el aviso de fallos y los textos. No cambia ningún comportamiento de la spec. `/status` muestra la antigüedad del último precio ("hace 12s") en lugar de una hora absoluta.

---

### Task 1: Toolchain, Makefile, harness de tests y `alert_logic`

**Files:**
- Create: `Makefile`
- Create: `test/test.h`
- Create: `btc_alerts/alert_logic.h`
- Create: `btc_alerts/alert_logic.cpp`
- Test: `test/test_alert_logic.cpp`

**Interfaces:**
- Consumes: nada.
- Produces:
  - `struct Threshold { double value = 0; bool armed = true; };`
  - `struct AlertState { Threshold low; Threshold high; };` con `operator==` / `operator!=`
  - `struct EvalResult { AlertState next; bool lowCrossed = false; bool highCrossed = false; };`
  - `EvalResult evaluate(const AlertState& state, double price, double margin);`
  - `AlertState commitAlerts(const EvalResult& result, bool lowSent, bool highSent);`
  - `enum class SetResult { Ok, Conflict };`
  - `SetResult setLow(AlertState& state, double value);` / `SetResult setHigh(AlertState& state, double value);`
  - Macros de test `CHECK`, `CHECK_STR`, `CHECK_NEAR`, `RUN`, función `testSummary()`.
  - Variables del Makefile `PURE_SRC` y `TESTS` (las tareas siguientes les agregan entradas).

- [ ] **Step 1: Instalar core ESP32 y librerías**

Run:
```bash
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
URL=https://espressif.github.io/arduino-esp32/package_esp32_index.json
"$CLI" core update-index --additional-urls $URL
"$CLI" core install esp32:esp32 --additional-urls $URL
"$CLI" lib update-index
"$CLI" lib install ArduinoJson UniversalTelegramBot
"$CLI" core list
"$CLI" lib list
ls ~/Documents/Arduino/libraries/ArduinoJson/src/ArduinoJson.h
```
Expected: `esp32:esp32` con versión ≥ 3.3.0; `ArduinoJson` 7.x y `UniversalTelegramBot` en `lib list`; el `ls` encuentra `ArduinoJson.h`. Si la carpeta de librerías no es `~/Documents/Arduino/libraries`, anotar la real (`"$CLI" config get directories.user`) y usarla como `ARDUINO_LIBS` en el Makefile.

- [ ] **Step 2: Crear el Makefile**

`Makefile`:
```make
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
PURE_SRC := $(SKETCH)/alert_logic.cpp
TESTS := alert_logic

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
```
(Las líneas de receta van indentadas con TAB.)

- [ ] **Step 3: Crear el harness de tests**

`test/test.h`:
```cpp
#pragma once
#include <cmath>
#include <cstdio>
#include <string>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                     \
  do {                                                                  \
    ++g_checks;                                                         \
    if (!(cond)) {                                                      \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)

#define CHECK_STR(actual, expected)                                     \
  do {                                                                  \
    ++g_checks;                                                         \
    std::string a_ = (actual), e_ = (expected);                         \
    if (a_ != e_) {                                                     \
      std::printf("  FAIL %s:%d:\n    got:      \"%s\"\n    expected: \"%s\"\n", \
                  __FILE__, __LINE__, a_.c_str(), e_.c_str());          \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)

#define CHECK_NEAR(actual, expected)                                    \
  do {                                                                  \
    ++g_checks;                                                         \
    double a_ = (actual), e_ = (expected);                              \
    if (std::fabs(a_ - e_) > 1e-6) {                                    \
      std::printf("  FAIL %s:%d: %.8f != %.8f\n", __FILE__, __LINE__, a_, e_); \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)

#define RUN(test_fn)                    \
  do {                                  \
    std::printf("- %s\n", #test_fn);    \
    test_fn();                          \
  } while (0)

static int testSummary() {
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
```

- [ ] **Step 4: Escribir los tests de `alert_logic` (fallan)**

`test/test_alert_logic.cpp`:
```cpp
#include "test.h"
#include "alert_logic.h"

static const double M = 0.005;

static AlertState withLow(double v) {
  AlertState s;
  s.low.value = v;
  return s;
}

static AlertState withHigh(double v) {
  AlertState s;
  s.high.value = v;
  return s;
}

static void test_low_fires_at_or_below_threshold() {
  EvalResult r = evaluate(withLow(90000), 90000, M);
  CHECK(r.lowCrossed);
  CHECK(!r.next.low.armed);
  CHECK(evaluate(withLow(90000), 89950, M).lowCrossed);
}

static void test_low_does_not_fire_above_threshold() {
  EvalResult r = evaluate(withLow(90000), 90001, M);
  CHECK(!r.lowCrossed);
  CHECK(r.next.low.armed);
}

static void test_low_fires_only_once_while_oscillating() {
  AlertState s = evaluate(withLow(90000), 89950, M).next;
  for (double p : {89900.0, 90300.0, 89999.0, 90449.0}) {
    EvalResult r = evaluate(s, p, M);
    CHECK(!r.lowCrossed);
    s = r.next;
  }
  CHECK(!s.low.armed);
}

static void test_low_rearms_above_margin_and_fires_again() {
  AlertState s = evaluate(withLow(90000), 89950, M).next;
  s = evaluate(s, 90451, M).next;  // > 90000 * 1.005 = 90450
  CHECK(s.low.armed);
  CHECK(evaluate(s, 89999, M).lowCrossed);
}

static void test_high_fires_at_or_above_threshold() {
  EvalResult r = evaluate(withHigh(130000), 130000, M);
  CHECK(r.highCrossed);
  CHECK(!r.next.high.armed);
  CHECK(evaluate(withHigh(130000), 130120, M).highCrossed);
}

static void test_high_does_not_fire_below_threshold() {
  EvalResult r = evaluate(withHigh(130000), 129999, M);
  CHECK(!r.highCrossed);
  CHECK(r.next.high.armed);
}

static void test_high_rearms_below_margin() {
  AlertState s = evaluate(withHigh(130000), 130120, M).next;
  s = evaluate(s, 129351, M).next;  // 130000 * 0.995 = 129350 -> todavía no
  CHECK(!s.high.armed);
  s = evaluate(s, 129349, M).next;
  CHECK(s.high.armed);
}

static void test_disabled_thresholds_never_fire() {
  AlertState s;
  EvalResult r = evaluate(s, 1, M);
  CHECK(!r.lowCrossed);
  CHECK(!r.highCrossed);
  r = evaluate(s, 1e9, M);
  CHECK(!r.lowCrossed);
  CHECK(!r.highCrossed);
}

static void test_thresholds_are_independent() {
  AlertState s;
  s.low.value = 90000;
  s.high.value = 130000;
  EvalResult r = evaluate(s, 89000, M);
  CHECK(r.lowCrossed);
  CHECK(!r.highCrossed);
  CHECK(r.next.high.armed);
}

static void test_commit_rearms_alerts_that_failed_to_send() {
  AlertState s;
  s.low.value = 90000;
  s.high.value = 130000;
  EvalResult r = evaluate(s, 89000, M);
  CHECK(commitAlerts(r, false, false).low.armed);   // envío falló -> reintentar
  CHECK(!commitAlerts(r, true, false).low.armed);   // envío OK -> disparado
  CHECK(commitAlerts(r, true, false).high.armed);   // high no se cruzó
}

static void test_commit_keeps_silent_rearm() {
  AlertState s = evaluate(withLow(90000), 89000, M).next;
  EvalResult r = evaluate(s, 91000, M);  // rearme, sin alerta
  CHECK(commitAlerts(r, false, false).low.armed);
}

static void test_set_low_arms_and_validates_against_high() {
  AlertState s;
  s.high.value = 130000;
  s.low.armed = false;
  CHECK(setLow(s, 90000) == SetResult::Ok);
  CHECK_NEAR(s.low.value, 90000);
  CHECK(s.low.armed);
  CHECK(setLow(s, 130000) == SetResult::Conflict);
  CHECK(setLow(s, 140000) == SetResult::Conflict);
  CHECK_NEAR(s.low.value, 90000);
  CHECK(setLow(s, 0) == SetResult::Ok);  // desactivar siempre se permite
  CHECK_NEAR(s.low.value, 0);
}

static void test_set_high_validates_against_low() {
  AlertState s;
  s.low.value = 90000;
  CHECK(setHigh(s, 90000) == SetResult::Conflict);
  CHECK(setHigh(s, 80000) == SetResult::Conflict);
  CHECK_NEAR(s.high.value, 0);
  CHECK(setHigh(s, 130000) == SetResult::Ok);
  CHECK_NEAR(s.high.value, 130000);
  AlertState t;  // low desactivado -> cualquier high vale
  CHECK(setHigh(t, 5) == SetResult::Ok);
}

static void test_new_threshold_already_crossed_fires_next_check() {
  AlertState s;
  setLow(s, 100000);
  CHECK(evaluate(s, 95000, M).lowCrossed);
}

static void test_equality() {
  AlertState a, b;
  CHECK(a == b);
  b.low.armed = false;
  CHECK(a != b);
}

int main() {
  RUN(test_low_fires_at_or_below_threshold);
  RUN(test_low_does_not_fire_above_threshold);
  RUN(test_low_fires_only_once_while_oscillating);
  RUN(test_low_rearms_above_margin_and_fires_again);
  RUN(test_high_fires_at_or_above_threshold);
  RUN(test_high_does_not_fire_below_threshold);
  RUN(test_high_rearms_below_margin);
  RUN(test_disabled_thresholds_never_fire);
  RUN(test_thresholds_are_independent);
  RUN(test_commit_rearms_alerts_that_failed_to_send);
  RUN(test_commit_keeps_silent_rearm);
  RUN(test_set_low_arms_and_validates_against_high);
  RUN(test_set_high_validates_against_low);
  RUN(test_new_threshold_already_crossed_fires_next_check);
  RUN(test_equality);
  return testSummary();
}
```

- [ ] **Step 5: Verificar que falla**

Run: `make test`
Expected: error de compilación `'alert_logic.h' file not found` (o falta `alert_logic.cpp`).

- [ ] **Step 6: Implementar `alert_logic`**

`btc_alerts/alert_logic.h`:
```cpp
#pragma once

// Un umbral de precio en USD. value == 0 significa desactivado.
struct Threshold {
  double value = 0;
  bool armed = true;
};

struct AlertState {
  Threshold low;
  Threshold high;
};

bool operator==(const Threshold& a, const Threshold& b);
bool operator==(const AlertState& a, const AlertState& b);
bool operator!=(const AlertState& a, const AlertState& b);

// Resultado de evaluar un precio: el nuevo estado (umbrales cruzados ya
// desarmados) y qué umbrales se cruzaron en esta evaluación.
struct EvalResult {
  AlertState next;
  bool lowCrossed = false;
  bool highCrossed = false;
};

// Regla de disparo y rearme con histéresis. margin es una fracción (0.005 = 0.5 %).
EvalResult evaluate(const AlertState& state, double price, double margin);

// Estado a guardar tras intentar enviar las alertas: un umbral cuya alerta no
// se pudo enviar vuelve a quedar armado para reintentar en el siguiente ciclo.
AlertState commitAlerts(const EvalResult& result, bool lowSent, bool highSent);

enum class SetResult { Ok, Conflict };

// Fija el umbral (0 = desactivar) y lo deja armado. Devuelve Conflict, sin
// modificar el estado, si ambos umbrales quedarían activos con low >= high.
SetResult setLow(AlertState& state, double value);
SetResult setHigh(AlertState& state, double value);
```

`btc_alerts/alert_logic.cpp`:
```cpp
#include "alert_logic.h"

bool operator==(const Threshold& a, const Threshold& b) {
  return a.value == b.value && a.armed == b.armed;
}

bool operator==(const AlertState& a, const AlertState& b) {
  return a.low == b.low && a.high == b.high;
}

bool operator!=(const AlertState& a, const AlertState& b) {
  return !(a == b);
}

EvalResult evaluate(const AlertState& state, double price, double margin) {
  EvalResult r;
  r.next = state;

  Threshold& low = r.next.low;
  if (low.value > 0) {
    if (low.armed && price <= low.value) {
      low.armed = false;
      r.lowCrossed = true;
    } else if (!low.armed && price > low.value * (1 + margin)) {
      low.armed = true;
    }
  }

  Threshold& high = r.next.high;
  if (high.value > 0) {
    if (high.armed && price >= high.value) {
      high.armed = false;
      r.highCrossed = true;
    } else if (!high.armed && price < high.value * (1 - margin)) {
      high.armed = true;
    }
  }

  return r;
}

AlertState commitAlerts(const EvalResult& result, bool lowSent, bool highSent) {
  AlertState s = result.next;
  if (result.lowCrossed && !lowSent) s.low.armed = true;
  if (result.highCrossed && !highSent) s.high.armed = true;
  return s;
}

SetResult setLow(AlertState& state, double value) {
  if (value > 0 && state.high.value > 0 && value >= state.high.value) {
    return SetResult::Conflict;
  }
  state.low.value = value;
  state.low.armed = true;
  return SetResult::Ok;
}

SetResult setHigh(AlertState& state, double value) {
  if (value > 0 && state.low.value > 0 && value <= state.low.value) {
    return SetResult::Conflict;
  }
  state.high.value = value;
  state.high.armed = true;
  return SetResult::Ok;
}
```

- [ ] **Step 7: Verificar que pasa**

Run: `make test`
Expected: `== test/bin/test_alert_logic`, la lista de tests y `N checks, 0 failures`; exit code 0.

- [ ] **Step 8: Commit**

```bash
git add Makefile test/test.h test/test_alert_logic.cpp btc_alerts/alert_logic.h btc_alerts/alert_logic.cpp
git commit -m "feat: add alert logic with hysteresis and host test harness"
```

---

### Task 2: `decimal` y `command_parser`

**Files:**
- Create: `btc_alerts/decimal.h`, `btc_alerts/decimal.cpp`
- Create: `btc_alerts/command_parser.h`, `btc_alerts/command_parser.cpp`
- Modify: `Makefile` (variables `PURE_SRC` y `TESTS`)
- Test: `test/test_decimal.cpp`, `test/test_command_parser.cpp`

**Interfaces:**
- Consumes: harness de Task 1.
- Produces:
  - `bool parsePositiveDecimal(const std::string& text, double& out);`
  - `enum class CommandType { SetLow, SetHigh, Status, Help, Unknown };`
  - `struct Command { CommandType type = CommandType::Unknown; bool valid = true; double value = 0; };`
  - `Command parseCommand(const std::string& text);`

- [ ] **Step 1: Registrar los módulos en el Makefile**

En `Makefile`, reemplazar:
```make
PURE_SRC := $(SKETCH)/alert_logic.cpp
TESTS := alert_logic
```
por:
```make
PURE_SRC := $(SKETCH)/alert_logic.cpp $(SKETCH)/decimal.cpp $(SKETCH)/command_parser.cpp
TESTS := alert_logic decimal command_parser
```

- [ ] **Step 2: Escribir los tests (fallan)**

`test/test_decimal.cpp`:
```cpp
#include "test.h"
#include "decimal.h"

static void test_accepts_integers_and_decimals() {
  double v = 0;
  CHECK(parsePositiveDecimal("90000", v));
  CHECK_NEAR(v, 90000);
  CHECK(parsePositiveDecimal("90000.5", v));
  CHECK_NEAR(v, 90000.5);
  CHECK(parsePositiveDecimal("89950.12000000", v));  // formato de Binance
  CHECK_NEAR(v, 89950.12);
  CHECK(parsePositiveDecimal("0.5", v));
  CHECK_NEAR(v, 0.5);
}

static void test_rejects_malformed_input() {
  double v = 123;
  for (const char* s : {"", "abc", "-5", "+5", "0", "0.000", "90,000", "90000.",
                        ".5", "1e5", " 90000", "90000 ", "9 0000", "1.2.3", "inf", "nan"}) {
    if (parsePositiveDecimal(s, v)) std::printf("  accepted: \"%s\"\n", s);
    CHECK(!parsePositiveDecimal(s, v));
  }
  CHECK_NEAR(v, 123);  // out no se modifica si falla
}

static void test_rejects_overflow() {
  double v = 0;
  CHECK(!parsePositiveDecimal(std::string(400, '9'), v));
}

int main() {
  RUN(test_accepts_integers_and_decimals);
  RUN(test_rejects_malformed_input);
  RUN(test_rejects_overflow);
  return testSummary();
}
```

`test/test_command_parser.cpp`:
```cpp
#include "test.h"
#include "command_parser.h"

static void test_set_low_and_high() {
  Command c = parseCommand("/low 90000");
  CHECK(c.type == CommandType::SetLow);
  CHECK(c.valid);
  CHECK_NEAR(c.value, 90000);

  c = parseCommand("/high 130000.5");
  CHECK(c.type == CommandType::SetHigh);
  CHECK(c.valid);
  CHECK_NEAR(c.value, 130000.5);
}

static void test_off_disables() {
  Command c = parseCommand("/low off");
  CHECK(c.type == CommandType::SetLow);
  CHECK(c.valid);
  CHECK_NEAR(c.value, 0);

  c = parseCommand("/high OFF");
  CHECK(c.type == CommandType::SetHigh);
  CHECK(c.valid);
  CHECK_NEAR(c.value, 0);
}

static void test_case_spaces_and_bot_suffix() {
  for (const char* s : {"/LOW 90000", "  /low   90000  ", "/low@hjpBtcAlertsBot 90000", "/Low\t90000"}) {
    Command c = parseCommand(s);
    CHECK(c.type == CommandType::SetLow);
    CHECK(c.valid);
    CHECK_NEAR(c.value, 90000);
  }
}

static void test_invalid_threshold_arguments() {
  for (const char* s : {"/low", "/low abc", "/low -5", "/low 0", "/low 90,000",
                        "/low 90000 100", "/low 1e5", "/high", "/high 1,5"}) {
    Command c = parseCommand(s);
    CHECK(c.type == CommandType::SetLow || c.type == CommandType::SetHigh);
    if (c.valid) std::printf("  accepted: \"%s\"\n", s);
    CHECK(!c.valid);
  }
}

static void test_dot_thousands_separator_parses_as_decimal() {
  // "90.000" es un decimal válido (= 90); la confirmación al usuario lo hace evidente.
  Command c = parseCommand("/low 90.000");
  CHECK(c.valid);
  CHECK_NEAR(c.value, 90);
}

static void test_status_and_help() {
  CHECK(parseCommand("/status").type == CommandType::Status);
  CHECK(parseCommand("/Status@hjpBtcAlertsBot").type == CommandType::Status);
  CHECK(parseCommand("/help").type == CommandType::Help);
  CHECK(parseCommand("/start").type == CommandType::Help);
}

static void test_unknown() {
  for (const char* s : {"hola", "", "   ", "/foo", "low 90000", "/", "/lowx 90000"}) {
    CHECK(parseCommand(s).type == CommandType::Unknown);
  }
}

int main() {
  RUN(test_set_low_and_high);
  RUN(test_off_disables);
  RUN(test_case_spaces_and_bot_suffix);
  RUN(test_invalid_threshold_arguments);
  RUN(test_dot_thousands_separator_parses_as_decimal);
  RUN(test_status_and_help);
  RUN(test_unknown);
  return testSummary();
}
```

- [ ] **Step 3: Verificar que falla**

Run: `make test`
Expected: error de compilación `'decimal.h' file not found` / `'command_parser.h' file not found`.

- [ ] **Step 4: Implementar `decimal`**

`btc_alerts/decimal.h`:
```cpp
#pragma once
#include <string>

// Parsea un decimal positivo con formato estricto: dígitos con un punto decimal
// opcional seguido de dígitos ("90000", "90000.5"). Rechaza signos, comas,
// espacios, exponentes, cero y valores no finitos. Si falla, out no se modifica.
bool parsePositiveDecimal(const std::string& text, double& out);
```

`btc_alerts/decimal.cpp`:
```cpp
#include "decimal.h"

#include <cmath>
#include <cstdlib>

static bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

bool parsePositiveDecimal(const std::string& text, double& out) {
  size_t i = 0;
  size_t digitsBefore = 0;
  while (i < text.size() && isDigit(text[i])) {
    ++i;
    ++digitsBefore;
  }
  if (i < text.size() && text[i] == '.') {
    ++i;
    size_t digitsAfter = 0;
    while (i < text.size() && isDigit(text[i])) {
      ++i;
      ++digitsAfter;
    }
    if (digitsAfter == 0) return false;
  }
  if (digitsBefore == 0 || i != text.size()) return false;

  double value = std::strtod(text.c_str(), nullptr);
  if (!std::isfinite(value) || value <= 0) return false;
  out = value;
  return true;
}
```

- [ ] **Step 5: Implementar `command_parser`**

`btc_alerts/command_parser.h`:
```cpp
#pragma once
#include <string>

enum class CommandType { SetLow, SetHigh, Status, Help, Unknown };

struct Command {
  CommandType type = CommandType::Unknown;
  bool valid = true;  // false si /low o /high traen un argumento inválido
  double value = 0;   // SetLow/SetHigh: precio en USD, 0 = desactivar
};

// Interpreta un mensaje de Telegram. Ignora mayúsculas en el comando, espacios
// sobrantes y el sufijo "@NombreDelBot".
Command parseCommand(const std::string& text);
```

`btc_alerts/command_parser.cpp`:
```cpp
#include "command_parser.h"

#include <cctype>
#include <vector>

#include "decimal.h"

static std::string toLower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

static std::vector<std::string> splitWords(const std::string& text) {
  std::vector<std::string> words;
  std::string current;
  for (char c : text) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!current.empty()) {
        words.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) words.push_back(current);
  return words;
}

Command parseCommand(const std::string& text) {
  Command cmd;
  std::vector<std::string> words = splitWords(text);
  if (words.empty() || words[0][0] != '/') return cmd;

  std::string name = toLower(words[0].substr(0, words[0].find('@')));
  if (name == "/status") {
    cmd.type = CommandType::Status;
    return cmd;
  }
  if (name == "/help" || name == "/start") {
    cmd.type = CommandType::Help;
    return cmd;
  }
  if (name != "/low" && name != "/high") return cmd;

  cmd.type = name == "/low" ? CommandType::SetLow : CommandType::SetHigh;
  if (words.size() != 2) {
    cmd.valid = false;
    return cmd;
  }
  if (toLower(words[1]) == "off") {
    cmd.value = 0;
    return cmd;
  }
  cmd.valid = parsePositiveDecimal(words[1], cmd.value);
  return cmd;
}
```

- [ ] **Step 6: Verificar que pasa**

Run: `make test`
Expected: los tres ejecutables terminan con `0 failures`, sin líneas `accepted:`.

- [ ] **Step 7: Commit**

```bash
git add Makefile btc_alerts/decimal.* btc_alerts/command_parser.* test/test_decimal.cpp test/test_command_parser.cpp
git commit -m "feat: add strict decimal parsing and Telegram command parser"
```

---

### Task 3: `price_parser` y `price_health`

**Files:**
- Create: `btc_alerts/price_parser.h`, `btc_alerts/price_parser.cpp`
- Create: `btc_alerts/price_health.h`, `btc_alerts/price_health.cpp`
- Modify: `Makefile` (variables `PURE_SRC` y `TESTS`)
- Test: `test/test_price_parser.cpp`, `test/test_price_health.cpp`

**Interfaces:**
- Consumes: `parsePositiveDecimal` (Task 2); ArduinoJson v7 instalado en Task 1.
- Produces:
  - `bool parseBinancePrice(const char* json, double& out);`
  - `bool parseCoinbasePrice(const char* json, double& out);`
  - `struct PriceHealth { uint32_t lastOkMs = 0; bool failAlertSent = false; };`
  - `bool shouldSendFailAlert(const PriceHealth& h, uint32_t nowMs, uint32_t failAfterMs);`

- [ ] **Step 1: Registrar los módulos en el Makefile**

En `Makefile`, reemplazar las líneas `PURE_SRC` y `TESTS` por:
```make
PURE_SRC := $(SKETCH)/alert_logic.cpp $(SKETCH)/decimal.cpp $(SKETCH)/command_parser.cpp \
            $(SKETCH)/price_parser.cpp $(SKETCH)/price_health.cpp
TESTS := alert_logic decimal command_parser price_parser price_health
```

- [ ] **Step 2: Escribir los tests (fallan)**

`test/test_price_parser.cpp`:
```cpp
#include "test.h"
#include "price_parser.h"

static void test_binance_ok() {
  double p = 0;
  CHECK(parseBinancePrice(R"({"symbol":"BTCUSDT","price":"89950.12000000"})", p));
  CHECK_NEAR(p, 89950.12);
}

static void test_coinbase_ok() {
  double p = 0;
  CHECK(parseCoinbasePrice(R"({"data":{"amount":"89950.125","base":"BTC","currency":"USD"}})", p));
  CHECK_NEAR(p, 89950.125);
}

static void test_numeric_price_field_is_accepted() {
  double p = 0;
  CHECK(parseBinancePrice(R"({"price":89950.5})", p));
  CHECK_NEAR(p, 89950.5);
}

static void test_binance_failures() {
  double p = 42;
  for (const char* body : {
           R"({"code":0,"msg":"Service unavailable from a restricted location"})",
           R"({"symbol":"BTCUSDT","price":"0.00000000"})",
           R"({"symbol":"BTCUSDT","price":"-1"})",
           R"({"symbol":"BTCUSDT","price":"abc"})",
           R"({"symbol":"BTCUSDT","price":null})",
           R"({"price":0})",
           R"({"price":)",
           "<html><body>502 Bad Gateway</body></html>",
           ""}) {
    if (parseBinancePrice(body, p)) std::printf("  accepted: %s\n", body);
    CHECK(!parseBinancePrice(body, p));
  }
  CHECK_NEAR(p, 42);
}

static void test_coinbase_failures() {
  double p = 42;
  for (const char* body : {
           R"({"errors":[{"id":"not_found","message":"Invalid currency"}]})",
           R"({"data":{"base":"BTC","currency":"USD"}})",
           R"({"data":{"amount":"","base":"BTC","currency":"USD"}})",
           R"({"data":"x"})",
           "not json"}) {
    if (parseCoinbasePrice(body, p)) std::printf("  accepted: %s\n", body);
    CHECK(!parseCoinbasePrice(body, p));
  }
  CHECK_NEAR(p, 42);
}

int main() {
  RUN(test_binance_ok);
  RUN(test_coinbase_ok);
  RUN(test_numeric_price_field_is_accepted);
  RUN(test_binance_failures);
  RUN(test_coinbase_failures);
  return testSummary();
}
```

`test/test_price_health.cpp`:
```cpp
#include "test.h"
#include "price_health.h"

static const uint32_t TEN_MIN = 10UL * 60 * 1000;

static void test_no_alert_before_threshold() {
  PriceHealth h;
  h.lastOkMs = 1000;
  CHECK(!shouldSendFailAlert(h, 1000 + TEN_MIN - 1, TEN_MIN));
}

static void test_alert_after_threshold() {
  PriceHealth h;
  h.lastOkMs = 1000;
  CHECK(shouldSendFailAlert(h, 1000 + TEN_MIN, TEN_MIN));
}

static void test_alert_only_once() {
  PriceHealth h;
  h.lastOkMs = 1000;
  h.failAlertSent = true;
  CHECK(!shouldSendFailAlert(h, 1000 + 3 * TEN_MIN, TEN_MIN));
}

static void test_survives_millis_overflow() {
  PriceHealth h;
  h.lastOkMs = 0xFFFFFF00u;                         // justo antes del desborde
  CHECK(!shouldSendFailAlert(h, 0x00000100u, TEN_MIN));  // pasaron 512 ms
  CHECK(shouldSendFailAlert(h, TEN_MIN, TEN_MIN));       // pasaron 10 min + 256 ms
}

int main() {
  RUN(test_no_alert_before_threshold);
  RUN(test_alert_after_threshold);
  RUN(test_alert_only_once);
  RUN(test_survives_millis_overflow);
  return testSummary();
}
```

- [ ] **Step 3: Verificar que falla**

Run: `make test`
Expected: error de compilación `'price_parser.h' file not found`.

- [ ] **Step 4: Implementar `price_parser`**

`btc_alerts/price_parser.h`:
```cpp
#pragma once

// Extraen el precio de las respuestas JSON de cada API. Devuelven false (sin
// modificar out) si el JSON es inválido, falta el campo o el precio no es un
// número positivo finito.

// Binance: {"symbol":"BTCUSDT","price":"89950.12000000"}
bool parseBinancePrice(const char* json, double& out);

// Coinbase: {"data":{"amount":"89950.12","base":"BTC","currency":"USD"}}
bool parseCoinbasePrice(const char* json, double& out);
```

`btc_alerts/price_parser.cpp`:
```cpp
#include "price_parser.h"

#include <ArduinoJson.h>
#include <cmath>

#include "decimal.h"

static bool priceFromJson(JsonVariantConst v, double& out) {
  if (v.is<const char*>()) return parsePositiveDecimal(v.as<const char*>(), out);
  if (v.is<double>()) {
    double d = v.as<double>();
    if (!std::isfinite(d) || d <= 0) return false;
    out = d;
    return true;
  }
  return false;
}

bool parseBinancePrice(const char* json, double& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonVariantConst root = doc.as<JsonVariantConst>();
  return priceFromJson(root["price"], out);
}

bool parseCoinbasePrice(const char* json, double& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonVariantConst root = doc.as<JsonVariantConst>();
  return priceFromJson(root["data"]["amount"], out);
}
```

- [ ] **Step 5: Implementar `price_health`**

`btc_alerts/price_health.h`:
```cpp
#pragma once
#include <cstdint>

// Seguimiento de fallos sostenidos al obtener el precio.
struct PriceHealth {
  uint32_t lastOkMs = 0;       // millis() del último precio obtenido (o del arranque)
  bool failAlertSent = false;  // ya se avisó del fallo actual
};

// true si hay que enviar el aviso "no puedo obtener el precio": pasaron al menos
// failAfterMs desde el último precio y todavía no se avisó. La resta sin signo
// de 32 bits tolera el desborde de millis().
bool shouldSendFailAlert(const PriceHealth& h, uint32_t nowMs, uint32_t failAfterMs);
```

`btc_alerts/price_health.cpp`:
```cpp
#include "price_health.h"

bool shouldSendFailAlert(const PriceHealth& h, uint32_t nowMs, uint32_t failAfterMs) {
  uint32_t elapsed = nowMs - h.lastOkMs;
  return !h.failAlertSent && elapsed >= failAfterMs;
}
```

- [ ] **Step 6: Verificar que pasa**

Run: `make test`
Expected: los cinco ejecutables con `0 failures`, sin líneas `accepted:`.

- [ ] **Step 7: Commit**

```bash
git add Makefile btc_alerts/price_parser.* btc_alerts/price_health.* test/test_price_parser.cpp test/test_price_health.cpp
git commit -m "feat: add price JSON parsing and sustained-failure tracking"
```

---

### Task 4: `messages` (textos y formateo)

**Files:**
- Create: `btc_alerts/messages.h`, `btc_alerts/messages.cpp`
- Modify: `Makefile` (variables `PURE_SRC` y `TESTS`)
- Test: `test/test_messages.cpp`

**Interfaces:**
- Consumes: `AlertState`, `Threshold` (Task 1).
- Produces:
  - `std::string formatUsd(double value);`
  - `std::string formatDuration(uint32_t seconds);`
  - `std::string lowAlertMsg(double threshold, double price);`
  - `std::string highAlertMsg(double threshold, double price);`
  - `std::string onlineMsg(bool hasPrice, double price);`
  - `std::string priceFailMsg();`
  - `std::string priceRecoveredMsg(double price);`
  - `std::string helpMsg();`
  - `std::string usageMsg(bool isLow);`
  - `std::string thresholdSetMsg(bool isLow, double value);`
  - `std::string conflictMsg(bool isLow, double other);`
  - `struct StatusInfo { bool hasPrice = false; double price = 0; uint32_t priceAgeSec = 0; AlertState state; int rssi = 0; uint32_t uptimeSec = 0; };`
  - `std::string statusMsg(const StatusInfo& info);`

- [ ] **Step 1: Registrar el módulo en el Makefile**

En `Makefile`, reemplazar las líneas `PURE_SRC` y `TESTS` por:
```make
PURE_SRC := $(SKETCH)/alert_logic.cpp $(SKETCH)/decimal.cpp $(SKETCH)/command_parser.cpp \
            $(SKETCH)/price_parser.cpp $(SKETCH)/price_health.cpp $(SKETCH)/messages.cpp
TESTS := alert_logic decimal command_parser price_parser price_health messages
```

- [ ] **Step 2: Escribir los tests (fallan)**

`test/test_messages.cpp`:
```cpp
#include "test.h"
#include "messages.h"

static void test_format_usd() {
  CHECK_STR(formatUsd(90000), "90,000");
  CHECK_STR(formatUsd(89950.12), "89,950.12");
  CHECK_STR(formatUsd(89950.5), "89,950.50");
  CHECK_STR(formatUsd(1234567.891), "1,234,567.89");
  CHECK_STR(formatUsd(999), "999");
  CHECK_STR(formatUsd(999.999), "1,000");
  CHECK_STR(formatUsd(90), "90");
  CHECK_STR(formatUsd(0.5), "0.50");
  CHECK_STR(formatUsd(1e21), "1,000,000,000,000,000,000,000");
}

static void test_format_duration() {
  CHECK_STR(formatDuration(0), "0s");
  CHECK_STR(formatDuration(45), "45s");
  CHECK_STR(formatDuration(125), "2m");
  CHECK_STR(formatDuration(3725), "1h 2m");
  CHECK_STR(formatDuration(86400), "1d 0h 0m");
  CHECK_STR(formatDuration(183900), "2d 3h 5m");
}

static void test_alert_messages_match_spec() {
  CHECK_STR(lowAlertMsg(90000, 89950), "🔻 BTC bajó de 90,000 USD → ahora 89,950 USD");
  CHECK_STR(highAlertMsg(130000, 130120), "🚀 BTC superó 130,000 USD → ahora 130,120 USD");
  CHECK_STR(onlineMsg(true, 89950.12), "🟢 Online — BTC = 89,950.12 USD");
  CHECK_STR(onlineMsg(false, 0), "🟢 Online — todavía sin precio de BTC");
  CHECK_STR(priceFailMsg(), "⚠️ No puedo obtener el precio de BTC");
  CHECK_STR(priceRecoveredMsg(90000), "✅ Precio recuperado: 90,000 USD");
}

static void test_threshold_confirmation_shows_value() {
  CHECK_STR(thresholdSetMsg(true, 90000), "✅ Mínimo fijado en 90,000 USD");
  CHECK_STR(thresholdSetMsg(false, 130000), "✅ Máximo fijado en 130,000 USD");
  CHECK_STR(thresholdSetMsg(true, 0), "✅ Mínimo desactivado");
  CHECK_STR(thresholdSetMsg(false, 0), "✅ Máximo desactivado");
  // "/low 90.000" -> 90: la confirmación deja el error a la vista.
  CHECK_STR(thresholdSetMsg(true, 90), "✅ Mínimo fijado en 90 USD");
}

static void test_usage_and_conflict() {
  CHECK_STR(usageMsg(true),
            "❌ Uso: /low <precio en USD> o /low off\nEjemplo: /low 90000 (sin separador de miles)");
  CHECK_STR(usageMsg(false),
            "❌ Uso: /high <precio en USD> o /high off\nEjemplo: /high 130000 (sin separador de miles)");
  CHECK_STR(conflictMsg(true, 130000), "❌ El mínimo debe ser menor que el máximo (130,000 USD)");
  CHECK_STR(conflictMsg(false, 90000), "❌ El máximo debe ser mayor que el mínimo (90,000 USD)");
}

static void test_help_lists_all_commands() {
  std::string h = helpMsg();
  for (const char* cmd : {"/low <precio>", "/low off", "/high <precio>", "/high off", "/status", "/help"}) {
    if (h.find(cmd) == std::string::npos) std::printf("  missing: %s\n", cmd);
    CHECK(h.find(cmd) != std::string::npos);
  }
}

static void test_status_with_price() {
  StatusInfo info;
  info.hasPrice = true;
  info.price = 89950.12;
  info.priceAgeSec = 12;
  info.state.low.value = 90000;
  info.state.low.armed = false;
  info.state.high.value = 0;
  info.rssi = -62;
  info.uptimeSec = 183900;
  CHECK_STR(statusMsg(info),
            "📊 BTC: 89,950.12 USD (hace 12s)\n"
            "🔻 Mínimo: 90,000 USD (disparado)\n"
            "🚀 Máximo: desactivado\n"
            "📶 WiFi: -62 dBm\n"
            "⏱ Encendido hace 2d 3h 5m");
}

static void test_status_without_price() {
  StatusInfo info;
  info.state.high.value = 130000;
  info.rssi = -70;
  info.uptimeSec = 5;
  CHECK_STR(statusMsg(info),
            "📊 BTC: sin datos todavía\n"
            "🔻 Mínimo: desactivado\n"
            "🚀 Máximo: 130,000 USD (armado)\n"
            "📶 WiFi: -70 dBm\n"
            "⏱ Encendido hace 5s");
}

int main() {
  RUN(test_format_usd);
  RUN(test_format_duration);
  RUN(test_alert_messages_match_spec);
  RUN(test_threshold_confirmation_shows_value);
  RUN(test_usage_and_conflict);
  RUN(test_help_lists_all_commands);
  RUN(test_status_with_price);
  RUN(test_status_without_price);
  return testSummary();
}
```

- [ ] **Step 3: Verificar que falla**

Run: `make test`
Expected: error de compilación `'messages.h' file not found`.

- [ ] **Step 4: Implementar `messages`**

`btc_alerts/messages.h`:
```cpp
#pragma once
#include <cstdint>
#include <string>

#include "alert_logic.h"

// "90,000", "89,950.12": separador de miles con coma y hasta 2 decimales
// (se omiten si son .00).
std::string formatUsd(double value);

// "45s", "2m", "1h 2m", "2d 3h 5m"
std::string formatDuration(uint32_t seconds);

std::string lowAlertMsg(double threshold, double price);
std::string highAlertMsg(double threshold, double price);
std::string onlineMsg(bool hasPrice, double price);
std::string priceFailMsg();
std::string priceRecoveredMsg(double price);
std::string helpMsg();
std::string usageMsg(bool isLow);
std::string thresholdSetMsg(bool isLow, double value);
// other: valor del umbral contrario que impide el cambio.
std::string conflictMsg(bool isLow, double other);

struct StatusInfo {
  bool hasPrice = false;
  double price = 0;
  uint32_t priceAgeSec = 0;
  AlertState state;
  int rssi = 0;
  uint32_t uptimeSec = 0;
};

std::string statusMsg(const StatusInfo& info);
```

`btc_alerts/messages.cpp`:
```cpp
#include "messages.h"

#include <cstdio>
#include <vector>

std::string formatUsd(double value) {
  int len = std::snprintf(nullptr, 0, "%.2f", value);
  std::vector<char> buf(len + 1);
  std::snprintf(buf.data(), buf.size(), "%.2f", value);
  std::string fixed(buf.data());

  size_t dot = fixed.find('.');
  std::string intPart = fixed.substr(0, dot);
  std::string frac = fixed.substr(dot + 1);

  std::string out;
  size_t n = intPart.size();
  for (size_t i = 0; i < n; ++i) {
    out += intPart[i];
    size_t remaining = n - 1 - i;
    if (remaining > 0 && remaining % 3 == 0) out += ',';
  }
  if (frac != "00") out += "." + frac;
  return out;
}

std::string formatDuration(uint32_t seconds) {
  if (seconds < 60) return std::to_string(seconds) + "s";
  uint32_t d = seconds / 86400;
  uint32_t h = (seconds % 86400) / 3600;
  uint32_t m = (seconds % 3600) / 60;
  std::string out;
  if (d) out += std::to_string(d) + "d ";
  if (d || h) out += std::to_string(h) + "h ";
  out += std::to_string(m) + "m";
  return out;
}

std::string lowAlertMsg(double threshold, double price) {
  return "🔻 BTC bajó de " + formatUsd(threshold) + " USD → ahora " + formatUsd(price) + " USD";
}

std::string highAlertMsg(double threshold, double price) {
  return "🚀 BTC superó " + formatUsd(threshold) + " USD → ahora " + formatUsd(price) + " USD";
}

std::string onlineMsg(bool hasPrice, double price) {
  if (!hasPrice) return "🟢 Online — todavía sin precio de BTC";
  return "🟢 Online — BTC = " + formatUsd(price) + " USD";
}

std::string priceFailMsg() {
  return "⚠️ No puedo obtener el precio de BTC";
}

std::string priceRecoveredMsg(double price) {
  return "✅ Precio recuperado: " + formatUsd(price) + " USD";
}

std::string helpMsg() {
  return "🤖 Comandos:\n"
         "/low <precio> — avisa si BTC baja de ese precio (USD)\n"
         "/low off — desactiva el mínimo\n"
         "/high <precio> — avisa si BTC sube de ese precio (USD)\n"
         "/high off — desactiva el máximo\n"
         "/status — precio actual y configuración\n"
         "/help — esta ayuda\n"
         "Ejemplo: /low 90000";
}

std::string usageMsg(bool isLow) {
  std::string cmd = isLow ? "/low" : "/high";
  std::string example = isLow ? "90000" : "130000";
  return "❌ Uso: " + cmd + " <precio en USD> o " + cmd + " off\n"
         "Ejemplo: " + cmd + " " + example + " (sin separador de miles)";
}

std::string thresholdSetMsg(bool isLow, double value) {
  std::string name = isLow ? "Mínimo" : "Máximo";
  if (value == 0) return "✅ " + name + " desactivado";
  return "✅ " + name + " fijado en " + formatUsd(value) + " USD";
}

std::string conflictMsg(bool isLow, double other) {
  if (isLow) return "❌ El mínimo debe ser menor que el máximo (" + formatUsd(other) + " USD)";
  return "❌ El máximo debe ser mayor que el mínimo (" + formatUsd(other) + " USD)";
}

static std::string thresholdLine(const Threshold& t) {
  if (t.value == 0) return "desactivado";
  return formatUsd(t.value) + " USD (" + (t.armed ? "armado" : "disparado") + ")";
}

std::string statusMsg(const StatusInfo& info) {
  std::string s = info.hasPrice
      ? "📊 BTC: " + formatUsd(info.price) + " USD (hace " + formatDuration(info.priceAgeSec) + ")"
      : "📊 BTC: sin datos todavía";
  s += "\n🔻 Mínimo: " + thresholdLine(info.state.low);
  s += "\n🚀 Máximo: " + thresholdLine(info.state.high);
  s += "\n📶 WiFi: " + std::to_string(info.rssi) + " dBm";
  s += "\n⏱ Encendido hace " + formatDuration(info.uptimeSec);
  return s;
}
```

- [ ] **Step 5: Verificar que pasa**

Run: `make test`
Expected: los seis ejecutables con `0 failures`, sin líneas `missing:`.

- [ ] **Step 6: Commit**

```bash
git add Makefile btc_alerts/messages.* test/test_messages.cpp
git commit -m "feat: add user-facing messages and number formatting"
```

---

### Task 5: Firmware — config, NVS, fuente de precios, bot y `loop()`

**Files:**
- Create: `btc_alerts/config.h`
- Create: `btc_alerts/secrets.example.h`
- Create: `btc_alerts/settings_store.h`, `btc_alerts/settings_store.cpp`
- Create: `btc_alerts/price_source.h`, `btc_alerts/price_source.cpp`
- Create: `btc_alerts/telegram_bot.h`, `btc_alerts/telegram_bot.cpp`
- Create: `btc_alerts/btc_alerts.ino`
- Create (local, no se versiona): `btc_alerts/secrets.h`

**Interfaces:**
- Consumes: todo lo producido en Tasks 1–4 (firmas exactas arriba).
- Produces:
  - `void loadAlertState(AlertState& state);` / `void saveAlertState(const AlertState& state);`
  - `bool fetchBtcUsd(double& price);`
  - `void botBegin();` / `bool botSend(const std::string& text);`
  - `using CommandHandler = std::string (*)(const std::string& text);` / `void botPoll(CommandHandler handler);`

Estos módulos dependen del core ESP32, así que no tienen tests en el host. Se verifican compilando (`make compile`) y en el dispositivo (Task 6).

- [ ] **Step 1: Crear `config.h` y `secrets.example.h`**

`btc_alerts/config.h`:
```cpp
#pragma once
#include <cstdint>

// Intervalos (ms)
constexpr unsigned long PRICE_INTERVAL_MS = 60UL * 1000;
constexpr unsigned long BOT_POLL_INTERVAL_MS = 3UL * 1000;
constexpr unsigned long PRICE_FAIL_ALERT_MS = 10UL * 60 * 1000;
constexpr unsigned long WIFI_RETRY_MS = 30UL * 1000;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20UL * 1000;
constexpr unsigned long NTP_TIMEOUT_MS = 10UL * 1000;
constexpr uint16_t HTTP_TIMEOUT_MS = 10000;

// Histéresis de rearme (fracción del umbral)
constexpr double REARM_MARGIN = 0.005;

// Fuentes de precio
constexpr const char* BINANCE_URL = "https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT";
constexpr const char* COINBASE_URL = "https://api.coinbase.com/v2/prices/BTC-USD/spot";

// NTP (necesario para validar certificados TLS)
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.google.com";
```

`btc_alerts/secrets.example.h`:
```cpp
#pragma once
// Copia este archivo a secrets.h (que no se sube a git) y completa los valores.

#define WIFI_SSID "nombre-de-tu-red"
#define WIFI_PASSWORD "clave-de-tu-red"

// Token de @BotFather y tu chat_id (ver getUpdates)
#define TELEGRAM_BOT_TOKEN "123456789:AAAA-BBBB"
#define TELEGRAM_CHAT_ID "123456789"
```

Crear `btc_alerts/secrets.h` local copiando la plantilla:
```bash
cp btc_alerts/secrets.example.h btc_alerts/secrets.h
git check-ignore btc_alerts/secrets.h
```
Expected: `git check-ignore` imprime `btc_alerts/secrets.h` (está ignorado).

- [ ] **Step 2: Crear `settings_store`**

`btc_alerts/settings_store.h`:
```cpp
#pragma once
#include "alert_logic.h"

// Persistencia de umbrales y estado armado/disparado en NVS.
// En el primer arranque (NVS vacío) deja los umbrales desactivados y armados.
void loadAlertState(AlertState& state);
void saveAlertState(const AlertState& state);
```

`btc_alerts/settings_store.cpp`:
```cpp
#include "settings_store.h"

#include <Arduino.h>
#include <Preferences.h>

static const char* NVS_NAMESPACE = "btcalert";

void loadAlertState(AlertState& state) {
  Preferences prefs;
  // Si el namespace no existe, begin() falla y los get*() devuelven el valor por defecto.
  prefs.begin(NVS_NAMESPACE, true);
  state.low.value = prefs.getDouble("low", 0);
  state.low.armed = prefs.getBool("lowArmed", true);
  state.high.value = prefs.getDouble("high", 0);
  state.high.armed = prefs.getBool("highArmed", true);
  prefs.end();
}

void saveAlertState(const AlertState& state) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    Serial.println("[nvs] no se pudo abrir el almacenamiento");
    return;
  }
  prefs.putDouble("low", state.low.value);
  prefs.putBool("lowArmed", state.low.armed);
  prefs.putDouble("high", state.high.value);
  prefs.putBool("highArmed", state.high.armed);
  prefs.end();
}
```

- [ ] **Step 3: Crear `price_source`**

`btc_alerts/price_source.h`:
```cpp
#pragma once

// Obtiene el precio BTC/USD: Binance y, si falla, Coinbase.
// Devuelve false si ninguna fuente entrega un precio válido.
bool fetchBtcUsd(double& price);
```

`btc_alerts/price_source.cpp`:
```cpp
#include "price_source.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "price_parser.h"

static bool httpGet(const char* url, String& body) {
  WiFiClientSecure client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    Serial.printf("[price] no se pudo iniciar %s\n", url);
    return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[price] %s -> HTTP %d\n", url, code);
    http.end();
    return false;
  }
  body = http.getString();
  http.end();
  return true;
}

bool fetchBtcUsd(double& price) {
  String body;
  if (httpGet(BINANCE_URL, body) && parseBinancePrice(body.c_str(), price)) return true;
  if (httpGet(COINBASE_URL, body) && parseCoinbasePrice(body.c_str(), price)) return true;
  return false;
}
```

- [ ] **Step 4: Crear `telegram_bot`**

`btc_alerts/telegram_bot.h`:
```cpp
#pragma once
#include <string>

// Inicializa el cliente TLS del bot con el bundle de certificados raíz del core.
void botBegin();

// Envía un mensaje al chat configurado. Devuelve false si falla.
bool botSend(const std::string& text);

// Recibe el texto de un mensaje y devuelve la respuesta a enviar.
using CommandHandler = std::string (*)(const std::string& text);

// Lee los mensajes nuevos y responde con handler() solo a los del chat
// configurado; los demás se ignoran (se registran en el log serie).
void botPoll(CommandHandler handler);
```

`btc_alerts/telegram_bot.cpp`:
```cpp
#include "telegram_bot.h"

#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

static WiFiClientSecure client;
static UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

void botBegin() {
  client.useBuiltinCACertBundle();
}

bool botSend(const std::string& text) {
  bool ok = bot.sendMessage(TELEGRAM_CHAT_ID, text.c_str());
  if (!ok) Serial.println("[bot] error enviando mensaje");
  return ok;
}

void botPoll(CommandHandler handler) {
  int count = bot.getUpdates(bot.last_message_received + 1);
  while (count > 0) {
    for (int i = 0; i < count; ++i) {
      const telegramMessage& msg = bot.messages[i];
      if (msg.chat_id != TELEGRAM_CHAT_ID) {
        Serial.printf("[bot] mensaje ignorado del chat %s\n", msg.chat_id.c_str());
        continue;
      }
      Serial.printf("[bot] comando: %s\n", msg.text.c_str());
      botSend(handler(msg.text.c_str()));
    }
    count = bot.getUpdates(bot.last_message_received + 1);
  }
}
```

- [ ] **Step 5: Crear el sketch principal**

`btc_alerts/btc_alerts.ino`:
```cpp
#include <WiFi.h>
#include <esp_timer.h>
#include <time.h>

#include "alert_logic.h"
#include "command_parser.h"
#include "config.h"
#include "messages.h"
#include "price_health.h"
#include "price_source.h"
#include "secrets.h"
#include "settings_store.h"
#include "telegram_bot.h"

static AlertState state;
static PriceHealth health;
static bool hasPrice = false;
static double lastPrice = 0;
static unsigned long lastPriceAtMs = 0;
static unsigned long lastPricePollMs = 0;
static unsigned long lastBotPollMs = 0;
static unsigned long lastWifiAttemptMs = 0;
static bool firstPricePoll = true;
static bool timeSynced = false;
static bool onlineSent = false;

static bool ensureWifi(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (now - lastWifiAttemptMs >= WIFI_RETRY_MS) {
    lastWifiAttemptMs = now;
    Serial.println("[wifi] reconectando...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  return false;
}

static bool syncTime() {
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
  unsigned long start = millis();
  while (time(nullptr) < 1700000000 && millis() - start < NTP_TIMEOUT_MS) delay(200);
  bool ok = time(nullptr) >= 1700000000;
  Serial.println(ok ? "[ntp] hora sincronizada" : "[ntp] sin respuesta, reintentando");
  return ok;
}

static void checkPrice(unsigned long now) {
  double price;
  if (!fetchBtcUsd(price)) {
    Serial.println("[price] no se pudo obtener el precio");
    if (shouldSendFailAlert(health, now, PRICE_FAIL_ALERT_MS) && botSend(priceFailMsg())) {
      health.failAlertSent = true;
    }
    return;
  }

  if (health.failAlertSent) botSend(priceRecoveredMsg(price));
  health.failAlertSent = false;
  health.lastOkMs = now;
  hasPrice = true;
  lastPrice = price;
  lastPriceAtMs = now;
  Serial.printf("[price] BTC = %.2f USD\n", price);

  EvalResult r = evaluate(state, price, REARM_MARGIN);
  bool lowSent = r.lowCrossed && botSend(lowAlertMsg(state.low.value, price));
  bool highSent = r.highCrossed && botSend(highAlertMsg(state.high.value, price));
  AlertState next = commitAlerts(r, lowSent, highSent);
  if (next != state) {
    state = next;
    saveAlertState(state);
  }
}

static std::string handleCommand(const std::string& text) {
  Command cmd = parseCommand(text);
  switch (cmd.type) {
    case CommandType::SetLow:
    case CommandType::SetHigh: {
      bool isLow = cmd.type == CommandType::SetLow;
      if (!cmd.valid) return usageMsg(isLow);
      AlertState next = state;
      SetResult res = isLow ? setLow(next, cmd.value) : setHigh(next, cmd.value);
      if (res == SetResult::Conflict) {
        return conflictMsg(isLow, isLow ? state.high.value : state.low.value);
      }
      state = next;
      saveAlertState(state);
      return thresholdSetMsg(isLow, cmd.value);
    }
    case CommandType::Status: {
      StatusInfo info;
      info.hasPrice = hasPrice;
      info.price = lastPrice;
      info.priceAgeSec = (millis() - lastPriceAtMs) / 1000;
      info.state = state;
      info.rssi = WiFi.RSSI();
      info.uptimeSec = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
      return statusMsg(info);
    }
    case CommandType::Help:
    case CommandType::Unknown:
    default:
      return helpMsg();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[boot] BTC Price Alerts");

  loadAlertState(state);
  Serial.printf("[boot] low=%.2f (%s) high=%.2f (%s)\n",
                state.low.value, state.low.armed ? "armado" : "disparado",
                state.high.value, state.high.armed ? "armado" : "disparado");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs = millis();
  Serial.print("[wifi] conectando");
  while (WiFi.status() != WL_CONNECTED && millis() - lastWifiAttemptMs < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " sin conexión (se reintentará)");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }

  botBegin();
  health.lastOkMs = millis();
}

void loop() {
  unsigned long now = millis();
  if (!ensureWifi(now)) {
    delay(100);
    return;
  }
  if (!timeSynced) {
    timeSynced = syncTime();
    if (!timeSynced) return;
  }

  if (firstPricePoll || now - lastPricePollMs >= PRICE_INTERVAL_MS) {
    firstPricePoll = false;
    lastPricePollMs = now;
    checkPrice(now);
  }

  if (!onlineSent) onlineSent = botSend(onlineMsg(hasPrice, lastPrice));

  if (now - lastBotPollMs >= BOT_POLL_INTERVAL_MS) {
    lastBotPollMs = now;
    botPoll(handleCommand);
  }

  delay(10);
}
```

- [ ] **Step 6: Compilar**

Run: `make compile`
Expected: termina sin errores e imprime `Sketch uses ... bytes` para `esp32:esp32:XIAO_ESP32C5`. Se aceptan advertencias de deprecación de `DynamicJsonDocument` dentro de `UniversalTelegramBot` (es la librería, no nuestro código). Si aparece un error de API del core (por ejemplo `useBuiltinCACertBundle` no existe), verificar la versión del core con `"$CLI" core list` (debe ser ≥ 3.3).

- [ ] **Step 7: Confirmar que los tests del host siguen pasando**

Run: `make test`
Expected: los seis ejecutables con `0 failures`.

- [ ] **Step 8: Commit**

```bash
git status --short   # secrets.h NO debe aparecer
git add btc_alerts/config.h btc_alerts/secrets.example.h btc_alerts/settings_store.* btc_alerts/price_source.* btc_alerts/telegram_bot.* btc_alerts/btc_alerts.ino
git commit -m "feat: add firmware glue for WiFi, NVS, price source and Telegram bot"
```

---

### Task 6: Verificación en el dispositivo y README

Esta tarea requiere al usuario: él completa el WiFi en `secrets.h`, conecta el XIAO y confirma lo que ve en Telegram.

**Files:**
- Modify (local, no se versiona): `btc_alerts/secrets.h`
- Create: `README.md`

**Interfaces:**
- Consumes: firmware completo de Task 5.
- Produces: firmware verificado en hardware; README con instrucciones de uso.

- [ ] **Step 1: Completar credenciales**

Poner en `btc_alerts/secrets.h` `TELEGRAM_BOT_TOKEN` y `TELEGRAM_CHAT_ID` (`"7646724335"`). El usuario completa `WIFI_SSID` y `WIFI_PASSWORD` él mismo (no deben pasar por el chat). Recomendarle regenerar el token con @BotFather (`/revoke`) antes, porque el actual quedó expuesto en la conversación.

- [ ] **Step 2: Flashear y observar el log**

Run: `make flash` y luego `make monitor` (salir con Ctrl+C).
Expected en serie: `[boot] BTC Price Alerts`, `[wifi] conectando... OK`, `[ntp] hora sincronizada`, `[price] BTC = <precio> USD`.
Expected en Telegram: `🟢 Online — BTC = <precio> USD`.
Si `make flash` no encuentra el puerto: mantener BOOT presionado, pulsar RESET, soltar BOOT y reintentar.

- [ ] **Step 3: Checklist manual en Telegram**

| Acción | Resultado esperado |
|---|---|
| `/status` | Precio, ambos umbrales "desactivado", RSSI y uptime |
| `/low 90,000` | Mensaje de uso con ejemplo |
| `/high 100` con `/low` desactivado | `✅ Máximo fijado en 100 USD` y en ≤60 s `🚀 BTC superó 100 USD → ahora …` |
| Esperar 2–3 min | No llega una segunda alerta HIGH |
| `/high off` | `✅ Máximo desactivado` |
| `/low <precio actual + 5000>` | Confirmación y en ≤60 s `🔻 BTC bajó de …` |
| `/status` | Mínimo "(disparado)" |
| Desenchufar y volver a enchufar el XIAO | `🟢 Online …`; **no** se repite la alerta LOW; `/status` conserva el mínimo "(disparado)" |
| `/low off` | `✅ Mínimo desactivado` |
| `/low 90000` y luego `/high 80000` | `❌ El máximo debe ser mayor que el mínimo (90,000 USD)` |
| `hola` | Ayuda |

- [ ] **Step 4: Escribir el README**

`README.md`:
````markdown
# BTC Price Alerts — XIAO ESP32-C5

Vigila el precio BTC/USD (Binance, con Coinbase de respaldo) y avisa por Telegram
cuando cruza un mínimo o un máximo configurables desde el chat.

## Requisitos

- Seeed Studio XIAO ESP32-C5 y cable USB-C
- Arduino IDE 2.x (trae `arduino-cli`)
- Un bot de Telegram (@BotFather → `/newbot`)

## Instalación

```bash
make setup                                          # core ESP32 + librerías
cp btc_alerts/secrets.example.h btc_alerts/secrets.h
# editar secrets.h: WiFi, token del bot y chat_id
make flash
make monitor                                        # log serie (Ctrl+C para salir)
```

Para obtener tu `chat_id`: escríbele al bot y abre
`https://api.telegram.org/bot<TOKEN>/getUpdates` → `"chat":{"id": ...}`.

## Comandos del bot

| Comando | Efecto |
|---|---|
| `/low 90000` | Avisa si BTC baja de 90 000 USD |
| `/high 130000` | Avisa si BTC sube de 130 000 USD |
| `/low off`, `/high off` | Desactiva el umbral |
| `/status` | Precio actual, umbrales y estado |
| `/help` | Ayuda |

Escribe los precios sin separador de miles. Cada cruce avisa una vez; la alerta
se rearma cuando el precio vuelve al rango con un margen de 0.5 %.

## Desarrollo

```bash
make test      # tests de la lógica en el Mac (clang++)
make compile   # compila el firmware
```
````

- [ ] **Step 5: Commit**

```bash
git status --short   # secrets.h NO debe aparecer
git add README.md
git commit -m "docs: add README with setup and bot commands"
```

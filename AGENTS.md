# AGENTS.md

Guía para cualquier persona o agente de IA que modifique este proyecto. Para qué hace el
proyecto y cómo se usa, ver [README.md](README.md). El diseño original está en
`docs/superpowers/specs/`.

## Comandos

```bash
make test      # tests del host (clang++); deben pasar antes de cada commit
make compile   # compila el firmware; debe compilar sin warnings de nuestro código
make flash     # compila y sube al XIAO conectado por USB
make monitor   # log serie
make setup     # instala core esp32 y librerías con el arduino-cli del Arduino IDE
```

- El `arduino-cli` es el que trae Arduino IDE 2.x (ruta en `ARDUINO_CLI` del `Makefile`). No instales
  otro: el IDE y la CLI comparten `~/Library/Arduino15` y `~/Documents/Arduino/libraries`.
- FQBN: `esp32:esp32:XIAO_ESP32C5:CDCOnBoot=cdc`. Core `esp32:esp32` ≥ 3.3.
- `make compile` exige `btc_alerts/secrets.h`; créalo desde `secrets.example.h` si no existe.
- Los diagnósticos de clangd del editor sobre archivos de `btc_alerts/` (flags `-march=rv32...`,
  `_ansi.h` no encontrado) son falsos positivos del toolchain RISC-V; la verdad es `make compile`.

## Arquitectura: puro vs. hardware

La regla central del proyecto: **toda decisión vive en un módulo puro con tests**.

| Tipo | Módulos | Reglas |
|---|---|---|
| Puro | `alert_logic`, `command_parser`, `decimal`, `price_parser`, `price_health`, `messages` | Sin `Arduino.h`, sin `String`, sin APIs del ESP32. Usan `std::string` y `<cstdint>`. Compilan en macOS y en el ESP32. Tienen `test/test_<modulo>.cpp` |
| Hardware | `settings_store`, `price_source`, `telegram_bot`, `btc_alerts.ino` | Capa fina sobre el core: hacen E/S y delegan cada decisión a un módulo puro. Se verifican con `make compile` y en el dispositivo |

Si una función de hardware empieza a contener un `if` que decide algo (umbrales, textos, reintentos,
parseo), extráelo a un módulo puro y testéalo.

### Agregar un módulo puro

1. Crea `btc_alerts/<modulo>.h/.cpp` y `test/test_<modulo>.cpp`.
2. Agrega el `.cpp` a `PURE_SRC` y el nombre a `TESTS` en el `Makefile`.
3. El Arduino builder compila automáticamente todo `.cpp` de `btc_alerts/`; no hay que registrarlo
   en otro lugar.

### Agregar un comando de Telegram

1. Nuevo valor en `CommandType` y su parseo en `command_parser.cpp`, con tests.
2. Textos de respuesta en `messages.cpp`, con tests de la cadena exacta; añádelo a `helpMsg()`.
3. Despacho en `handleCommand()` de `btc_alerts.ino`.
4. Prueba manual en el dispositivo.

### Agregar una fuente de precio

1. `parse<Fuente>Price(const char* json, double& out)` en `price_parser`, con tests que incluyan
   respuestas de error reales de esa API (bloqueos, JSON truncado, HTML, precio `"0"`).
2. Añádela a la cadena de respaldo en `fetchBtcUsd()`.

## Flujo de trabajo

- **TDD en módulos puros**: escribe el test, míralo fallar, implementa, míralo pasar. Un test que
  pasa antes de implementar no prueba nada.
- **Cambios de firmware**: `make test && make compile` siempre; si cambia el comportamiento
  observable, flashea y verifícalo en el dispositivo con el log serie y Telegram.
- Los bugs se corrigen con un test que lo reproduce primero. Si el bug solo existe en el dispositivo
  (librería, red), reprodúcelo en el hardware antes y después del arreglo.
- Trabaja en una rama; `main` debe compilar y pasar los tests en cada commit.

## Reglas de firmware

- **Tiempo**: compara siempre con resta sin signo, `now - last >= interval`. Nunca
  `now >= last + interval`: falla cuando `millis()` da la vuelta (~49.7 días). En código puro usa
  `uint32_t`, porque `unsigned long` mide 64 bits en macOS y 32 en el ESP32.
- **TLS**: `client.useBuiltinCACertBundle()` en todo cliente HTTPS. Prohibido `setInsecure()`.
  La hora debe estar sincronizada por NTP antes de cualquier conexión TLS.
- **Bloqueo**: `loop()` debe volver rápido. Toda E/S tiene timeout (`HTTP_TIMEOUT_MS`); no agregues
  esperas sin límite.
- **NVS**: escribe solo cuando el estado cambia (`next != state`), para no desgastar la flash.
- **Alertas**: un umbral solo queda disparado si el envío tuvo éxito (`commitAlerts`). No rompas esa
  garantía: una alerta perdida es el peor fallo posible de este proyecto.
- **Entrada de red**: todo dato externo (precio, comando) se valida en un módulo puro. Un precio que
  no sea un número finito > 0 es un fallo, nunca un precio.
- **Seguridad**: el bot solo atiende a `TELEGRAM_CHAT_ID`. No agregues comandos que salten ese filtro.
- **UniversalTelegramBot**: `bot.maxMessageLength` debe seguir alto (16384). Con el valor por
  defecto (1500), un mensaje largo deja `getUpdates` atascado hasta 24 h.

## Estilo de código

- C++17. Formato del código existente: 2 espacios, llaves en la misma línea, `camelCase` para
  funciones y variables, `PascalCase` para tipos, `UPPER_SNAKE_CASE` para constantes de `config.h`.
- Constantes ajustables en `config.h`, nunca números mágicos en el código.
- Funciones cortas con nombres que expliquen qué hacen; si necesitas un comentario para decir qué
  hace algo, mejora el nombre.

### Comentarios

- **En inglés.**
- **Solo el porqué, nunca el qué**: el código ya dice qué hace. Un comentario explica una razón que
  no se deduce leyendo el código (un comportamiento de una librería, un límite del hardware, una
  decisión deliberada que parece un error).
- **Solo si es estrictamente necesario.** Ante la duda, no lo escribas.

```cpp
// Bien: explica por qué existe la línea
// The library truncates responses at 1500 chars by default; a single long or
// emoji-heavy message then fails to parse and wedges getUpdates on that update.
bot.maxMessageLength = 16384;

// Mal: repite el código
// Set the max message length to 16384
bot.maxMessageLength = 16384;
```

### Idiomas

| Qué | Idioma |
|---|---|
| Identificadores, comentarios y mensajes de commit | Inglés |
| Textos que ve el usuario en Telegram (`messages.cpp`) y log serie | Español |
| README, AGENTS.md y documentos de `docs/` | Español |

Los textos de Telegram tienen tests con la cadena exacta: si cambias uno, actualiza su test.

## Commits

- [Conventional Commits](https://www.conventionalcommits.org/): `feat:`, `fix:`, `docs:`, `test:`,
  `refactor:`, `chore:`.
- Un cambio lógico por commit, con tests en el mismo commit que el código.
- Sin trailers de atribución a IA (`Co-Authored-By: ...`).
- Nunca hagas commit de `btc_alerts/secrets.h`; revisa `git status` antes de cada commit.

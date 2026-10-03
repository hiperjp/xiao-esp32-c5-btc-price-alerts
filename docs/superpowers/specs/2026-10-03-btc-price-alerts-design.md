# BTC Price Alerts — XIAO ESP32-C5 — Diseño

Fecha: 2026-10-03

## Objetivo

Un Seeed Studio XIAO ESP32-C5 conectado a WiFi monitorea el precio de BTC en USD y
envía una alerta por Telegram cuando el precio cae por debajo de un umbral inferior
o sube por encima de un umbral superior. Los umbrales se configuran desde Telegram
y persisten en flash.

**Criterios de éxito**

- Una alerta llega al celular (Telegram) dentro de ~60 s de cruzar un umbral.
- Cada cruce produce una sola alerta; no hay spam si el precio oscila en el borde.
- Los umbrales y el estado de las alertas sobreviven a reinicios.
- Solo el dueño (un `chat_id` configurado) puede consultar o cambiar el bot.

## Decisiones

| Tema | Decisión |
|---|---|
| Canal de alerta | Telegram Bot (polling con `getUpdates`, sin webhook) |
| Configuración de umbrales | Comandos de Telegram, persistidos en NVS (`Preferences`) |
| Comportamiento de disparo | Una vez por cruce + rearme con histéresis (0.5 %) |
| Moneda / fuente | USD; Binance `BTCUSDT` principal, Coinbase `BTC-USD` respaldo |
| Toolchain | `arduino-cli` incluido en Arduino IDE 2.x, core `esp32:esp32` ≥ 3.3 |
| Placa (FQBN) | `esp32:esp32:XIAO_ESP32C5` |
| Librerías | `UniversalTelegramBot`, `ArduinoJson` (v7) |
| Credenciales | `secrets.h` (fuera de git); se versiona `secrets.example.h` |

Fuera de alcance (YAGNI): portal de configuración WiFi, e-mail, página web local,
múltiples criptomonedas o monedas, histórico de precios, pantalla/LED de estado.

## Arquitectura

```
Makefile                    → compile / flash / monitor / test (usa el arduino-cli del IDE)
btc_alerts/                 → carpeta del sketch (requisito de Arduino: mismo nombre que el .ino)
  btc_alerts.ino            → setup() + loop(): solo orquesta
  config.h                  → intervalos, margen, URLs, timeouts (valores por defecto)
  secrets.example.h         → plantilla; se copia a secrets.h (ignorado por git)
  alert_logic.h/.cpp        → máquina de estados pura (sin red ni hardware)
  command_parser.h/.cpp     → parseo/validación de comandos de texto (puro)
  price_parser.h/.cpp       → parseo del JSON de Binance/Coinbase (puro, ArduinoJson)
  price_source.h/.cpp       → HTTPS GET Binance → Coinbase; usa price_parser
  settings_store.h/.cpp     → lectura/escritura de umbrales y estado en NVS
  telegram_bot.h/.cpp       → envío de mensajes, polling y despacho de comandos
test/
  test_*.cpp                → tests en el host (clang++), sin framework externo
```

Los módulos puros (`alert_logic`, `command_parser`, `price_parser`) no incluyen
`Arduino.h` y compilan tanto en el ESP32 como en macOS. En ellos se concentran las
decisiones con riesgo de alertas falsas o perdidas, y se prueban en el host.

### Flujo de `loop()`

1. Si el WiFi está caído, reintenta la conexión (no bloqueante) y no hace nada más.
2. Cada `BOT_POLL_INTERVAL_MS` (≈3 s): consulta comandos nuevos de Telegram y responde.
3. Cada `PRICE_INTERVAL_MS` (60 s): obtiene el precio. Si lo consigue, se lo pasa a
   `alert_logic`; si hay alertas, las envía y persiste el nuevo estado.

Al arrancar: conecta WiFi, sincroniza hora por NTP (necesaria para validar TLS),
carga configuración desde NVS y envía `🟢 Online — BTC = <precio> USD`.

## Lógica de alertas (`alert_logic`)

Estado por umbral: valor (`0` = desactivado) y bandera `armed`.

| Umbral | Dispara si | Se rearma si |
|---|---|---|
| LOW | `armed && price <= low` | `!armed && price > low * (1 + margin)` |
| HIGH | `armed && price >= high` | `!armed && price < high * (1 - margin)` |

- `margin` = 0.005 por defecto (`config.h`).
- El disparo pone `armed = false`; el rearme es silencioso.
- Fijar un umbral nuevo (o desactivarlo) lo deja `armed = true`. Si el precio ya
  está del otro lado, la alerta llega en el siguiente chequeo.
- La función recibe el estado y el precio y devuelve el nuevo estado más la lista
  de eventos (`LOW_CROSSED`, `HIGH_CROSSED`). No envía nada ni toca NVS.
- El estado completo (umbrales + banderas) se persiste en NVS cuando cambia.

## Fuente de precios

- Principal: `GET https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT`
  → `{"symbol":"BTCUSDT","price":"..."}`.
- Respaldo: `GET https://api.coinbase.com/v2/prices/BTC-USD/spot`
  → `{"data":{"amount":"...","base":"BTC","currency":"USD"}}`.
- El precio debe parsear como número finito > 0; si no, es un fallo.
- TLS verificado con el bundle de certificados raíz del core (sin `setInsecure()`).
- Timeout por petición: 10 s.
- Binance puede responder 451 en algunos países; el respaldo lo cubre.

**Fallos sostenidos:** si no hay precio durante `PRICE_FAIL_ALERT_MS` (10 min)
seguidos, se envía una vez `⚠️ No puedo obtener el precio de BTC`. Cuando vuelve a
obtenerse, se envía `✅ Precio recuperado: <precio> USD`.

## Bot de Telegram

- Solo procesa mensajes cuyo `chat_id` coincide con `TELEGRAM_CHAT_ID`; el resto
  se ignora en silencio (se registra en el log serie).
- Comandos:

| Comando | Efecto |
|---|---|
| `/low <n>` | Fija umbral inferior (USD) |
| `/low off` | Desactiva umbral inferior |
| `/high <n>` | Fija umbral superior (USD) |
| `/high off` | Desactiva umbral superior |
| `/status` | Precio actual, hora del último precio, umbrales con estado (armado/disparado), RSSI WiFi, uptime |
| `/help`, `/start` | Lista de comandos |

- Validación de `<n>`: número decimal positivo (dígitos con un punto decimal
  opcional). Se rechazan valores no numéricos, cero o negativos.
- Si ambos umbrales están activos, debe cumplirse `low < high`; si no, se responde
  con un error y no se cambia nada.
- Comando desconocido → respuesta con la ayuda.
- Formato de alertas:
  - `🔻 BTC bajó de 90,000 USD → ahora 89,950 USD`
  - `🚀 BTC superó 130,000 USD → ahora 130,120 USD`

## Manejo de errores

| Situación | Comportamiento |
|---|---|
| WiFi caído | Reconexión periódica; no se consultan precios ni Telegram mientras tanto |
| Falla Binance | Se intenta Coinbase en el mismo ciclo |
| Fallan ambas fuentes | Se salta el ciclo; aviso tras 10 min seguidos (una vez) |
| Falla envío a Telegram | Se registra en serie; una alerta no enviada no marca el umbral como disparado, así que se reintenta en el siguiente ciclo |
| NVS vacío (primer arranque) | Umbrales desactivados (`0`), ambos armados |

## Pruebas

1. **Host (macOS, `make test`)**: `clang++` compila los módulos puros con tests
   basados en `assert`. ArduinoJson se usa como header-only desde la carpeta de
   librerías de Arduino.
   - `alert_logic`: disparo LOW/HIGH, no-redisparo, rearme exacto en el margen,
     umbral desactivado, cambio de umbral rearma, ambos umbrales a la vez.
   - `command_parser`: `/low 90000`, `/low 90000.5`, `/low off`, `/high`, valores
     inválidos (`abc`, `-5`, `0`, vacío), `low >= high`, comando desconocido.
   - `price_parser`: JSON real de Binance y Coinbase, JSON roto, campo faltante,
     precio no numérico.
2. **Compilación**: `make compile` con FQBN `esp32:esp32:XIAO_ESP32C5`.
3. **Dispositivo (manual, guiado)**: flashear, ver log serie, recibir `🟢 Online`,
   `/status`, forzar alerta con `/low` por encima del precio actual, verificar que
   no se repite, `/low off`, reiniciar y comprobar que los umbrales persisten.

## Configuración por el usuario

1. Crear el bot con @BotFather (`/newbot`) → token.
2. Enviar un mensaje al bot y abrir `https://api.telegram.org/bot<TOKEN>/getUpdates`
   → `chat.id`.
3. `cp btc_alerts/secrets.example.h btc_alerts/secrets.h` y completar WiFi, token y
   chat_id.
4. Conectar el XIAO por USB-C y ejecutar `make flash`.

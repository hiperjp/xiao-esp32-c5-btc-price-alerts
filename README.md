# BTC Price Alerts — XIAO ESP32-C5

Firmware para un [Seeed Studio XIAO ESP32-C5](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32C5-p-6609.html)
que vigila el precio de Bitcoin en USD y te avisa por **Telegram** cuando cruza un
mínimo o un máximo. Los umbrales se configuran desde el propio chat y se guardan en
la flash del dispositivo.

```
🔻 BTC bajó de 60,000 USD → ahora 59,950 USD
🚀 BTC superó 80,000 USD → ahora 80,120 USD
```

## Características

- Consulta el precio cada 60 s en **Binance** (`BTCUSDT`), con **Coinbase** (`BTC-USD`) como respaldo.
- Alerta **una sola vez por cruce**, con rearme por histéresis del 0.5 %: si el precio oscila
  justo en el borde no recibes una avalancha de mensajes.
- Umbrales configurables desde Telegram (`/low`, `/high`, `/status`) y persistentes entre reinicios.
- Si una alerta no se puede enviar (Telegram caído, WiFi inestable), se reintenta en el siguiente ciclo.
- Si no consigue el precio durante 10 minutos te avisa una vez, y otra vez cuando se recupera.
- Solo responde a tu `chat_id`; los mensajes de otras personas se ignoran.
- HTTPS verificado con el bundle de certificados raíz del core ESP32 (sin `setInsecure()`).
- Reconexión automática al WiFi (2.4 GHz o 5 GHz).

## Hardware

- Seeed Studio XIAO ESP32-C5
- Cable USB-C (datos) para flashear; después basta cualquier cargador USB-C.

No requiere componentes adicionales.

## Puesta en marcha

### 1. Herramientas

Se usa el `arduino-cli` que viene dentro de **Arduino IDE 2.x**, así que el core y las librerías
quedan compartidos con el IDE. Con el IDE instalado:

```bash
make setup
```

Instala el core `esp32:esp32` (≥ 3.3) y las librerías `ArduinoJson` (v7) y `UniversalTelegramBot`.

Si tu IDE está en otra ruta: `make setup ARDUINO_CLI=/ruta/a/arduino-cli`.

### 2. Bot de Telegram

1. En Telegram, habla con **@BotFather** → `/newbot` → guarda el **token**.
2. Abre el chat con tu bot nuevo, toca **Iniciar** y envíale cualquier mensaje.
3. Abre `https://api.telegram.org/bot<TOKEN>/getUpdates` en el navegador y busca
   `"chat":{"id": 123456789, ...}`: ese número es tu **chat_id**.

### 3. Credenciales

```bash
cp btc_alerts/secrets.example.h btc_alerts/secrets.h
```

Edita `btc_alerts/secrets.h` con tu WiFi, el token y el chat_id. Este archivo está en
`.gitignore`; nunca lo subas a un repositorio.

### 4. Flashear

Conecta el XIAO por USB-C:

```bash
make flash      # compila y sube el firmware
make monitor    # log serie (Ctrl+C para salir)
```

Al arrancar verás algo como:

```
[boot] BTC Price Alerts
[boot] low=0.00 (armado) high=0.00 (armado)
[wifi] conectando.... OK
[wifi] IP 192.168.1.85, RSSI -34 dBm
[ntp] hora sincronizada
[price] BTC = 84664.01 USD
```

y en Telegram llegará `🟢 Online — BTC = 84,664.01 USD`.

## Uso

| Comando | Efecto |
|---|---|
| `/low 60000` | Avisa si BTC baja de 60 000 USD |
| `/high 80000` | Avisa si BTC sube de 80 000 USD |
| `/low off` | Desactiva el mínimo |
| `/high off` | Desactiva el máximo |
| `/status` | Precio actual, umbrales con su estado, señal WiFi y tiempo encendido |
| `/help` | Lista de comandos |

- Escribe los precios **sin separador de miles** (`60000`, no `60,000`). Se aceptan decimales con punto
  (`60000.5`). La confirmación muestra el valor entendido, así que `60.000` aparecerá como `60 USD`.
- Si ambos umbrales están activos, el mínimo debe ser menor que el máximo.
- Al fijar un umbral queda **armado**. Si el precio ya está del otro lado, la alerta llega en el siguiente
  chequeo (≤ 60 s).
- Cualquier otro texto responde con la ayuda.

### Cómo funciona el rearme

Cada umbral está **armado** (listo para avisar) o **disparado** (ya avisó). Con `low = 60 000`:

| Precio | Qué pasa |
|---|---|
| 59 950 | 🔻 Alerta. El mínimo queda disparado |
| 59 900 … 60 250 | Nada: está disparado y no supera el margen |
| 60 310 (> 60 000 × 1.005) | Se rearma en silencio |
| 59 990 | 🔻 Nueva alerta |

El máximo funciona igual pero en espejo: se rearma cuando el precio baja de `high × 0.995`.
El estado se guarda en flash, así que un reinicio no repite una alerta ya enviada.

## Configuración

Los valores por defecto están en `btc_alerts/config.h`:

| Constante | Valor | Uso |
|---|---|---|
| `PRICE_INTERVAL_MS` | 60 s | Frecuencia de consulta del precio |
| `BOT_POLL_INTERVAL_MS` | 3 s | Frecuencia de lectura de comandos de Telegram |
| `PRICE_FAIL_ALERT_MS` | 10 min | Tiempo sin precio antes de avisar |
| `REARM_MARGIN` | 0.005 | Margen de histéresis (0.5 %) |
| `HTTP_TIMEOUT_MS` | 10 s | Timeout de cada petición de precio |
| `WIFI_RETRY_MS` | 30 s | Pausa entre reintentos de conexión WiFi |

Cambia el valor, ejecuta `make flash` y listo.

## Desarrollo

```bash
make test      # tests de la lógica en el Mac (clang++), sin hardware
make compile   # compila el firmware para el XIAO ESP32-C5
make flash     # compila y sube (PORT=/dev/cu.xxx para elegir puerto)
make monitor   # log serie a 115200 baudios
make clean     # borra build/ y test/bin/
```

### Estructura

```
Makefile
btc_alerts/                 sketch Arduino (la carpeta debe llamarse igual que el .ino)
  btc_alerts.ino            setup()/loop(): WiFi, NTP, temporizadores y orquestación
  config.h                  intervalos, margen, URLs
  secrets.example.h         plantilla de credenciales
  alert_logic.*             disparo y rearme de umbrales            ┐
  command_parser.*          texto de Telegram → comando              │ módulos puros:
  decimal.*                 parseo estricto de números               │ sin Arduino.h,
  price_parser.*            JSON de Binance/Coinbase → precio        │ probados en el host
  price_health.*            cuándo avisar de fallos sostenidos       │
  messages.*                todos los textos y formato de números    ┘
  settings_store.*          persistencia en NVS (Preferences)
  price_source.*            HTTPS a Binance con respaldo en Coinbase
  telegram_bot.*            envío, polling y filtro por chat_id
test/
  test.h                    mini framework de asserts
  test_*.cpp                un ejecutable por módulo puro
docs/superpowers/           especificación y plan de implementación
```

Toda la lógica que decide *si* y *qué* avisar vive en los módulos puros y tiene tests. Los módulos
que tocan red, flash o el bot son capas finas sobre el core ESP32 y se verifican compilando y en
el dispositivo. Las convenciones de desarrollo están en [AGENTS.md](AGENTS.md).

## Solución de problemas

| Síntoma | Causa probable / solución |
|---|---|
| `make flash` no encuentra el puerto | Mantén **BOOT** presionado, pulsa **RESET**, suelta BOOT y reintenta. O indica el puerto: `make flash PORT=/dev/cu.usbmodemXXXX` |
| `Falta btc_alerts/secrets.h` | Copia la plantilla (paso 3 de la puesta en marcha) |
| `[wifi] conectando...` sin `OK` | SSID o clave incorrectos en `secrets.h`, o la red está fuera de alcance |
| `[ntp] sin respuesta` | La red bloquea NTP (puerto UDP 123); sin hora no se pueden validar los certificados HTTPS |
| `[price] ... -> HTTP 451` | Binance bloquea tu país. No hace falta hacer nada: se usa Coinbase automáticamente |
| El bot no responde | Revisa que `TELEGRAM_CHAT_ID` sea el tuyo; los mensajes de otros chats se ignoran (aparecen en el log como `mensaje ignorado`) |
| No llega `🟢 Online` | Token incorrecto o Telegram inaccesible; el log muestra `[bot] error enviando mensaje` |

## Seguridad

- `secrets.h` contiene la clave del WiFi y el token del bot: está en `.gitignore`.
- Si el token se expone, revócalo con @BotFather (`/revoke`) y actualiza `secrets.h`.
- El bot ignora a cualquiera que no sea tu `chat_id`, aunque cualquiera puede encontrarlo y escribirle.

## Limitaciones conocidas

- El bot abre una conexión TLS a Telegram cada 3 s; el uso de memoria a lo largo de semanas no se
  ha medido todavía.
- Si Telegram tarda más de ~1.5 s en confirmar un envío, la librería puede reenviar y llegar una
  alerta duplicada.
- Un servidor de precios que acepte la conexión pero no complete el handshake TLS puede bloquear el
  bucle hasta ~2 minutos por fuente.
- Los cruces que ocurren y se revierten mientras el dispositivo está sin conexión no se notifican.

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

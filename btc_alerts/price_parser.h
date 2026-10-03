#pragma once

// Extraen el precio de las respuestas JSON de cada API. Devuelven false (sin
// modificar out) si el JSON es inválido, falta el campo o el precio no es un
// número positivo finito.

// Binance: {"symbol":"BTCUSDT","price":"89950.12000000"}
bool parseBinancePrice(const char* json, double& out);

// Coinbase: {"data":{"amount":"89950.12","base":"BTC","currency":"USD"}}
bool parseCoinbasePrice(const char* json, double& out);

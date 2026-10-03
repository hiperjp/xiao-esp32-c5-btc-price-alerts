#pragma once

// Obtiene el precio BTC/USD: Binance y, si falla, Coinbase.
// Devuelve false si ninguna fuente entrega un precio válido.
bool fetchBtcUsd(double& price);

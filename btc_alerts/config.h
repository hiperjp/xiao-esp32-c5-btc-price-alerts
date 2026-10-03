#pragma once
#include <cstdint>

constexpr unsigned long PRICE_INTERVAL_MS = 60UL * 1000;
constexpr unsigned long BOT_POLL_INTERVAL_MS = 3UL * 1000;
constexpr unsigned long PRICE_FAIL_ALERT_MS = 10UL * 60 * 1000;
constexpr unsigned long WIFI_RETRY_MS = 30UL * 1000;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20UL * 1000;
constexpr unsigned long NTP_TIMEOUT_MS = 10UL * 1000;
constexpr uint16_t HTTP_TIMEOUT_MS = 10000;

constexpr double REARM_MARGIN = 0.005;

constexpr const char* BINANCE_URL = "https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT";
constexpr const char* COINBASE_URL = "https://api.coinbase.com/v2/prices/BTC-USD/spot";

// TLS certificate validation needs the real time; the RTC starts at 1970.
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.google.com";

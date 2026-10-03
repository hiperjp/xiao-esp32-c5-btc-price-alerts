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

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

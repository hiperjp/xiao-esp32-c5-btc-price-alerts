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

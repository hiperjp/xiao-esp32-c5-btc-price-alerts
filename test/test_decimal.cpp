#include "test.h"
#include "decimal.h"

static void test_accepts_integers_and_decimals() {
  double v = 0;
  CHECK(parsePositiveDecimal("90000", v));
  CHECK_NEAR(v, 90000);
  CHECK(parsePositiveDecimal("90000.5", v));
  CHECK_NEAR(v, 90000.5);
  CHECK(parsePositiveDecimal("89950.12000000", v));  // Binance price format
  CHECK_NEAR(v, 89950.12);
  CHECK(parsePositiveDecimal("0.5", v));
  CHECK_NEAR(v, 0.5);
}

static void test_rejects_malformed_input() {
  double v = 123;
  for (const char* s : {"", "abc", "-5", "+5", "0", "0.000", "90,000", "90000.",
                        ".5", "1e5", " 90000", "90000 ", "9 0000", "1.2.3", "inf", "nan"}) {
    if (parsePositiveDecimal(s, v)) std::printf("  accepted: \"%s\"\n", s);
    CHECK(!parsePositiveDecimal(s, v));
  }
  CHECK_NEAR(v, 123);
}

static void test_rejects_overflow() {
  double v = 0;
  CHECK(!parsePositiveDecimal(std::string(400, '9'), v));
}

int main() {
  RUN(test_accepts_integers_and_decimals);
  RUN(test_rejects_malformed_input);
  RUN(test_rejects_overflow);
  return testSummary();
}

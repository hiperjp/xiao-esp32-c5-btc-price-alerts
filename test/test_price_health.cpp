#include "test.h"
#include "price_health.h"

static const uint32_t TEN_MIN = 10UL * 60 * 1000;

static void test_no_alert_before_threshold() {
  PriceHealth h;
  h.lastOkMs = 1000;
  CHECK(!shouldSendFailAlert(h, 1000 + TEN_MIN - 1, TEN_MIN));
}

static void test_alert_after_threshold() {
  PriceHealth h;
  h.lastOkMs = 1000;
  CHECK(shouldSendFailAlert(h, 1000 + TEN_MIN, TEN_MIN));
}

static void test_alert_only_once() {
  PriceHealth h;
  h.lastOkMs = 1000;
  h.failAlertSent = true;
  CHECK(!shouldSendFailAlert(h, 1000 + 3 * TEN_MIN, TEN_MIN));
}

static void test_survives_millis_overflow() {
  PriceHealth h;
  h.lastOkMs = 0xFFFFFF00u;  // just before millis() rolls over
  CHECK(!shouldSendFailAlert(h, 0x00000100u, TEN_MIN));  // 512 ms elapsed
  CHECK(shouldSendFailAlert(h, TEN_MIN, TEN_MIN));       // 10 min + 256 ms elapsed
}

int main() {
  RUN(test_no_alert_before_threshold);
  RUN(test_alert_after_threshold);
  RUN(test_alert_only_once);
  RUN(test_survives_millis_overflow);
  return testSummary();
}

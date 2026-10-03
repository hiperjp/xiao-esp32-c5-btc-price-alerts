#include "test.h"
#include "alert_logic.h"

static const double M = 0.005;

static AlertState withLow(double v) {
  AlertState s;
  s.low.value = v;
  return s;
}

static AlertState withHigh(double v) {
  AlertState s;
  s.high.value = v;
  return s;
}

static void test_low_fires_at_or_below_threshold() {
  EvalResult r = evaluate(withLow(90000), 90000, M);
  CHECK(r.lowCrossed);
  CHECK(!r.next.low.armed);
  CHECK(evaluate(withLow(90000), 89950, M).lowCrossed);
}

static void test_low_does_not_fire_above_threshold() {
  EvalResult r = evaluate(withLow(90000), 90001, M);
  CHECK(!r.lowCrossed);
  CHECK(r.next.low.armed);
}

static void test_low_fires_only_once_while_oscillating() {
  AlertState s = evaluate(withLow(90000), 89950, M).next;
  for (double p : {89900.0, 90300.0, 89999.0, 90449.0}) {
    EvalResult r = evaluate(s, p, M);
    CHECK(!r.lowCrossed);
    s = r.next;
  }
  CHECK(!s.low.armed);
}

static void test_low_rearms_above_margin_and_fires_again() {
  AlertState s = evaluate(withLow(90000), 89950, M).next;
  s = evaluate(s, 90451, M).next;  // > 90000 * 1.005 = 90450
  CHECK(s.low.armed);
  CHECK(evaluate(s, 89999, M).lowCrossed);
}

static void test_high_fires_at_or_above_threshold() {
  EvalResult r = evaluate(withHigh(130000), 130000, M);
  CHECK(r.highCrossed);
  CHECK(!r.next.high.armed);
  CHECK(evaluate(withHigh(130000), 130120, M).highCrossed);
}

static void test_high_does_not_fire_below_threshold() {
  EvalResult r = evaluate(withHigh(130000), 129999, M);
  CHECK(!r.highCrossed);
  CHECK(r.next.high.armed);
}

static void test_high_rearms_below_margin() {
  AlertState s = evaluate(withHigh(130000), 130120, M).next;
  s = evaluate(s, 129351, M).next;  // 130000 * 0.995 = 129350 -> todavía no
  CHECK(!s.high.armed);
  s = evaluate(s, 129349, M).next;
  CHECK(s.high.armed);
}

static void test_disabled_thresholds_never_fire() {
  AlertState s;
  EvalResult r = evaluate(s, 1, M);
  CHECK(!r.lowCrossed);
  CHECK(!r.highCrossed);
  r = evaluate(s, 1e9, M);
  CHECK(!r.lowCrossed);
  CHECK(!r.highCrossed);
}

static void test_thresholds_are_independent() {
  AlertState s;
  s.low.value = 90000;
  s.high.value = 130000;
  EvalResult r = evaluate(s, 89000, M);
  CHECK(r.lowCrossed);
  CHECK(!r.highCrossed);
  CHECK(r.next.high.armed);
}

static void test_commit_rearms_alerts_that_failed_to_send() {
  AlertState s;
  s.low.value = 90000;
  s.high.value = 130000;
  EvalResult r = evaluate(s, 89000, M);
  CHECK(commitAlerts(r, false, false).low.armed);   // envío falló -> reintentar
  CHECK(!commitAlerts(r, true, false).low.armed);   // envío OK -> disparado
  CHECK(commitAlerts(r, true, false).high.armed);   // high no se cruzó
}

static void test_commit_keeps_silent_rearm() {
  AlertState s = evaluate(withLow(90000), 89000, M).next;
  EvalResult r = evaluate(s, 91000, M);  // rearme, sin alerta
  CHECK(commitAlerts(r, false, false).low.armed);
}

static void test_set_low_arms_and_validates_against_high() {
  AlertState s;
  s.high.value = 130000;
  s.low.armed = false;
  CHECK(setLow(s, 90000) == SetResult::Ok);
  CHECK_NEAR(s.low.value, 90000);
  CHECK(s.low.armed);
  CHECK(setLow(s, 130000) == SetResult::Conflict);
  CHECK(setLow(s, 140000) == SetResult::Conflict);
  CHECK_NEAR(s.low.value, 90000);
  CHECK(setLow(s, 0) == SetResult::Ok);  // desactivar siempre se permite
  CHECK_NEAR(s.low.value, 0);
}

static void test_set_high_validates_against_low() {
  AlertState s;
  s.low.value = 90000;
  CHECK(setHigh(s, 90000) == SetResult::Conflict);
  CHECK(setHigh(s, 80000) == SetResult::Conflict);
  CHECK_NEAR(s.high.value, 0);
  CHECK(setHigh(s, 130000) == SetResult::Ok);
  CHECK_NEAR(s.high.value, 130000);
  AlertState t;  // low desactivado -> cualquier high vale
  CHECK(setHigh(t, 5) == SetResult::Ok);
}

static void test_new_threshold_already_crossed_fires_next_check() {
  AlertState s;
  setLow(s, 100000);
  CHECK(evaluate(s, 95000, M).lowCrossed);
}

static void test_equality() {
  AlertState a, b;
  CHECK(a == b);
  b.low.armed = false;
  CHECK(a != b);
}

int main() {
  RUN(test_low_fires_at_or_below_threshold);
  RUN(test_low_does_not_fire_above_threshold);
  RUN(test_low_fires_only_once_while_oscillating);
  RUN(test_low_rearms_above_margin_and_fires_again);
  RUN(test_high_fires_at_or_above_threshold);
  RUN(test_high_does_not_fire_below_threshold);
  RUN(test_high_rearms_below_margin);
  RUN(test_disabled_thresholds_never_fire);
  RUN(test_thresholds_are_independent);
  RUN(test_commit_rearms_alerts_that_failed_to_send);
  RUN(test_commit_keeps_silent_rearm);
  RUN(test_set_low_arms_and_validates_against_high);
  RUN(test_set_high_validates_against_low);
  RUN(test_new_threshold_already_crossed_fires_next_check);
  RUN(test_equality);
  return testSummary();
}

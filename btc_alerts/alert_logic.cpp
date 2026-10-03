#include "alert_logic.h"

bool operator==(const Threshold& a, const Threshold& b) {
  return a.value == b.value && a.armed == b.armed;
}

bool operator==(const AlertState& a, const AlertState& b) {
  return a.low == b.low && a.high == b.high;
}

bool operator!=(const AlertState& a, const AlertState& b) {
  return !(a == b);
}

EvalResult evaluate(const AlertState& state, double price, double margin) {
  EvalResult r;
  r.next = state;

  Threshold& low = r.next.low;
  if (low.value > 0) {
    if (low.armed && price <= low.value) {
      low.armed = false;
      r.lowCrossed = true;
    } else if (!low.armed && price > low.value * (1 + margin)) {
      low.armed = true;
    }
  }

  Threshold& high = r.next.high;
  if (high.value > 0) {
    if (high.armed && price >= high.value) {
      high.armed = false;
      r.highCrossed = true;
    } else if (!high.armed && price < high.value * (1 - margin)) {
      high.armed = true;
    }
  }

  return r;
}

AlertState commitAlerts(const EvalResult& result, bool lowSent, bool highSent) {
  AlertState s = result.next;
  if (result.lowCrossed && !lowSent) s.low.armed = true;
  if (result.highCrossed && !highSent) s.high.armed = true;
  return s;
}

SetResult setLow(AlertState& state, double value) {
  if (value > 0 && state.high.value > 0 && value >= state.high.value) {
    return SetResult::Conflict;
  }
  state.low.value = value;
  state.low.armed = true;
  return SetResult::Ok;
}

SetResult setHigh(AlertState& state, double value) {
  if (value > 0 && state.low.value > 0 && value <= state.low.value) {
    return SetResult::Conflict;
  }
  state.high.value = value;
  state.high.armed = true;
  return SetResult::Ok;
}

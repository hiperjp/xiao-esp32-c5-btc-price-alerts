#pragma once

struct Threshold {
  double value = 0;
  bool armed = true;
};

struct AlertState {
  Threshold low;
  Threshold high;
};

bool operator==(const Threshold& a, const Threshold& b);
bool operator==(const AlertState& a, const AlertState& b);
bool operator!=(const AlertState& a, const AlertState& b);

struct EvalResult {
  AlertState next;
  bool lowCrossed = false;
  bool highCrossed = false;
};

// The margin is hysteresis: a fired threshold only re-arms once the price moves
// clearly back into range, so hovering around the threshold doesn't spam alerts.
EvalResult evaluate(const AlertState& state, double price, double margin);

// Re-arms thresholds whose alert could not be sent, so the next cycle retries
// instead of silently losing the alert.
AlertState commitAlerts(const EvalResult& result, bool lowSent, bool highSent);

enum class SetResult { Ok, Conflict };

SetResult setLow(AlertState& state, double value);
SetResult setHigh(AlertState& state, double value);

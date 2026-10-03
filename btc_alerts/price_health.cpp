#include "price_health.h"

bool shouldSendFailAlert(const PriceHealth& h, uint32_t nowMs, uint32_t failAfterMs) {
  // Unsigned 32-bit subtraction stays correct across the millis() rollover (~49.7 days).
  uint32_t elapsed = nowMs - h.lastOkMs;
  return !h.failAlertSent && elapsed >= failAfterMs;
}

#include "price_health.h"

bool shouldSendFailAlert(const PriceHealth& h, uint32_t nowMs, uint32_t failAfterMs) {
  uint32_t elapsed = nowMs - h.lastOkMs;
  return !h.failAlertSent && elapsed >= failAfterMs;
}

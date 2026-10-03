#pragma once
#include <cstdint>

struct PriceHealth {
  uint32_t lastOkMs = 0;
  bool failAlertSent = false;
};

bool shouldSendFailAlert(const PriceHealth& h, uint32_t nowMs, uint32_t failAfterMs);

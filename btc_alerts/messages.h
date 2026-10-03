#pragma once
#include <cstdint>
#include <string>

#include "alert_logic.h"

// "90,000", "89,950.12": separador de miles con coma y hasta 2 decimales
// (se omiten si son .00).
std::string formatUsd(double value);

// "45s", "2m", "1h 2m", "2d 3h 5m"
std::string formatDuration(uint32_t seconds);

std::string lowAlertMsg(double threshold, double price);
std::string highAlertMsg(double threshold, double price);
std::string onlineMsg(bool hasPrice, double price);
std::string priceFailMsg();
std::string priceRecoveredMsg(double price);
std::string helpMsg();
std::string usageMsg(bool isLow);
std::string thresholdSetMsg(bool isLow, double value);
// other: valor del umbral contrario que impide el cambio.
std::string conflictMsg(bool isLow, double other);

struct StatusInfo {
  bool hasPrice = false;
  double price = 0;
  uint32_t priceAgeSec = 0;
  AlertState state;
  int rssi = 0;
  uint32_t uptimeSec = 0;
};

std::string statusMsg(const StatusInfo& info);

#pragma once
#include <cstdint>
#include <string>

#include "alert_logic.h"

enum class ResetCause { PowerOn, External, Software, Usb, Panic, Watchdog, Brownout, PowerGlitch, Other };

std::string formatUsd(double value);

std::string formatDuration(uint32_t seconds);

std::string lowAlertMsg(double threshold, double price);
std::string highAlertMsg(double threshold, double price);
std::string onlineMsg(bool hasPrice, double price, ResetCause cause);
std::string priceFailMsg();
std::string priceRecoveredMsg(double price);
std::string helpMsg();
std::string usageMsg(bool isLow);
std::string thresholdSetMsg(bool isLow, double value);
std::string conflictMsg(bool isLow, double otherThreshold);

struct StatusInfo {
  bool hasPrice = false;
  double price = 0;
  uint32_t priceAgeSec = 0;
  AlertState state;
  int rssi = 0;
  uint32_t uptimeSec = 0;
  uint32_t freeHeapBytes = 0;
  uint32_t minFreeHeapBytes = 0;
};

std::string statusMsg(const StatusInfo& info);

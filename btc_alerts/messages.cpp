#include "messages.h"

#include <cstdio>
#include <vector>

std::string formatUsd(double value) {
  int len = std::snprintf(nullptr, 0, "%.2f", value);
  std::vector<char> buf(len + 1);
  std::snprintf(buf.data(), buf.size(), "%.2f", value);
  std::string fixed(buf.data());

  size_t dot = fixed.find('.');
  std::string intPart = fixed.substr(0, dot);
  std::string frac = fixed.substr(dot + 1);

  std::string out;
  size_t n = intPart.size();
  for (size_t i = 0; i < n; ++i) {
    out += intPart[i];
    size_t remaining = n - 1 - i;
    if (remaining > 0 && remaining % 3 == 0) out += ',';
  }
  if (frac != "00") out += "." + frac;
  return out;
}

std::string formatDuration(uint32_t seconds) {
  if (seconds < 60) return std::to_string(seconds) + "s";
  uint32_t d = seconds / 86400;
  uint32_t h = (seconds % 86400) / 3600;
  uint32_t m = (seconds % 3600) / 60;
  std::string out;
  if (d) out += std::to_string(d) + "d ";
  if (d || h) out += std::to_string(h) + "h ";
  out += std::to_string(m) + "m";
  return out;
}

std::string lowAlertMsg(double threshold, double price) {
  return "🔻 BTC bajó de " + formatUsd(threshold) + " USD → ahora " + formatUsd(price) + " USD";
}

std::string highAlertMsg(double threshold, double price) {
  return "🚀 BTC superó " + formatUsd(threshold) + " USD → ahora " + formatUsd(price) + " USD";
}

std::string onlineMsg(bool hasPrice, double price) {
  if (!hasPrice) return "🟢 Online — todavía sin precio de BTC";
  return "🟢 Online — BTC = " + formatUsd(price) + " USD";
}

std::string priceFailMsg() {
  return "⚠️ No puedo obtener el precio de BTC";
}

std::string priceRecoveredMsg(double price) {
  return "✅ Precio recuperado: " + formatUsd(price) + " USD";
}

std::string helpMsg() {
  return "🤖 Comandos:\n"
         "/low <precio> — avisa si BTC baja de ese precio (USD)\n"
         "/low off — desactiva el mínimo\n"
         "/high <precio> — avisa si BTC sube de ese precio (USD)\n"
         "/high off — desactiva el máximo\n"
         "/status — precio actual y configuración\n"
         "/help — esta ayuda\n"
         "Ejemplo: /low 90000";
}

std::string usageMsg(bool isLow) {
  std::string cmd = isLow ? "/low" : "/high";
  std::string example = isLow ? "90000" : "130000";
  return "❌ Uso: " + cmd + " <precio en USD> o " + cmd + " off\n"
         "Ejemplo: " + cmd + " " + example + " (sin separador de miles)";
}

std::string thresholdSetMsg(bool isLow, double value) {
  std::string name = isLow ? "Mínimo" : "Máximo";
  if (value == 0) return "✅ " + name + " desactivado";
  return "✅ " + name + " fijado en " + formatUsd(value) + " USD";
}

std::string conflictMsg(bool isLow, double otherThreshold) {
  if (isLow) return "❌ El mínimo debe ser menor que el máximo (" + formatUsd(otherThreshold) + " USD)";
  return "❌ El máximo debe ser mayor que el mínimo (" + formatUsd(otherThreshold) + " USD)";
}

static std::string thresholdLine(const Threshold& t) {
  if (t.value == 0) return "desactivado";
  return formatUsd(t.value) + " USD (" + (t.armed ? "armado" : "disparado") + ")";
}

std::string statusMsg(const StatusInfo& info) {
  std::string s = info.hasPrice
      ? "📊 BTC: " + formatUsd(info.price) + " USD (hace " + formatDuration(info.priceAgeSec) + ")"
      : "📊 BTC: sin datos todavía";
  s += "\n🔻 Mínimo: " + thresholdLine(info.state.low);
  s += "\n🚀 Máximo: " + thresholdLine(info.state.high);
  s += "\n📶 WiFi: " + std::to_string(info.rssi) + " dBm";
  s += "\n⏱ Encendido hace " + formatDuration(info.uptimeSec);
  return s;
}

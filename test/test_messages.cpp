#include "test.h"
#include "messages.h"

static void test_format_usd() {
  CHECK_STR(formatUsd(90000), "90,000");
  CHECK_STR(formatUsd(89950.12), "89,950.12");
  CHECK_STR(formatUsd(89950.5), "89,950.50");
  CHECK_STR(formatUsd(1234567.891), "1,234,567.89");
  CHECK_STR(formatUsd(999), "999");
  CHECK_STR(formatUsd(999.999), "1,000");
  CHECK_STR(formatUsd(90), "90");
  CHECK_STR(formatUsd(0.5), "0.50");
  CHECK_STR(formatUsd(1e21), "1,000,000,000,000,000,000,000");
}

static void test_format_duration() {
  CHECK_STR(formatDuration(0), "0s");
  CHECK_STR(formatDuration(45), "45s");
  CHECK_STR(formatDuration(125), "2m");
  CHECK_STR(formatDuration(3725), "1h 2m");
  CHECK_STR(formatDuration(86400), "1d 0h 0m");
  CHECK_STR(formatDuration(183900), "2d 3h 5m");
}

static void test_alert_messages_match_spec() {
  CHECK_STR(lowAlertMsg(90000, 89950), "🔻 BTC bajó de 90,000 USD → ahora 89,950 USD");
  CHECK_STR(highAlertMsg(130000, 130120), "🚀 BTC superó 130,000 USD → ahora 130,120 USD");
  CHECK_STR(onlineMsg(true, 89950.12), "🟢 Online — BTC = 89,950.12 USD");
  CHECK_STR(onlineMsg(false, 0), "🟢 Online — todavía sin precio de BTC");
  CHECK_STR(priceFailMsg(), "⚠️ No puedo obtener el precio de BTC");
  CHECK_STR(priceRecoveredMsg(90000), "✅ Precio recuperado: 90,000 USD");
}

static void test_threshold_confirmation_shows_value() {
  CHECK_STR(thresholdSetMsg(true, 90000), "✅ Mínimo fijado en 90,000 USD");
  CHECK_STR(thresholdSetMsg(false, 130000), "✅ Máximo fijado en 130,000 USD");
  CHECK_STR(thresholdSetMsg(true, 0), "✅ Mínimo desactivado");
  CHECK_STR(thresholdSetMsg(false, 0), "✅ Máximo desactivado");
  // "/low 90.000" parses as 90; the confirmation must expose that.
  CHECK_STR(thresholdSetMsg(true, 90), "✅ Mínimo fijado en 90 USD");
}

static void test_usage_and_conflict() {
  CHECK_STR(usageMsg(true),
            "❌ Uso: /low <precio en USD> o /low off\nEjemplo: /low 90000 (sin separador de miles)");
  CHECK_STR(usageMsg(false),
            "❌ Uso: /high <precio en USD> o /high off\nEjemplo: /high 130000 (sin separador de miles)");
  CHECK_STR(conflictMsg(true, 130000), "❌ El mínimo debe ser menor que el máximo (130,000 USD)");
  CHECK_STR(conflictMsg(false, 90000), "❌ El máximo debe ser mayor que el mínimo (90,000 USD)");
}

static void test_help_lists_all_commands() {
  std::string h = helpMsg();
  for (const char* cmd : {"/low <precio>", "/low off", "/high <precio>", "/high off", "/status", "/help"}) {
    if (h.find(cmd) == std::string::npos) std::printf("  missing: %s\n", cmd);
    CHECK(h.find(cmd) != std::string::npos);
  }
}

static void test_status_with_price() {
  StatusInfo info;
  info.hasPrice = true;
  info.price = 89950.12;
  info.priceAgeSec = 12;
  info.state.low.value = 90000;
  info.state.low.armed = false;
  info.state.high.value = 0;
  info.rssi = -62;
  info.uptimeSec = 183900;
  CHECK_STR(statusMsg(info),
            "📊 BTC: 89,950.12 USD (hace 12s)\n"
            "🔻 Mínimo: 90,000 USD (disparado)\n"
            "🚀 Máximo: desactivado\n"
            "📶 WiFi: -62 dBm\n"
            "⏱ Encendido hace 2d 3h 5m");
}

static void test_status_without_price() {
  StatusInfo info;
  info.state.high.value = 130000;
  info.rssi = -70;
  info.uptimeSec = 5;
  CHECK_STR(statusMsg(info),
            "📊 BTC: sin datos todavía\n"
            "🔻 Mínimo: desactivado\n"
            "🚀 Máximo: 130,000 USD (armado)\n"
            "📶 WiFi: -70 dBm\n"
            "⏱ Encendido hace 5s");
}

int main() {
  RUN(test_format_usd);
  RUN(test_format_duration);
  RUN(test_alert_messages_match_spec);
  RUN(test_threshold_confirmation_shows_value);
  RUN(test_usage_and_conflict);
  RUN(test_help_lists_all_commands);
  RUN(test_status_with_price);
  RUN(test_status_without_price);
  return testSummary();
}

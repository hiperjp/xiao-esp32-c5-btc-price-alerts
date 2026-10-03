#include <WiFi.h>
#include <esp_timer.h>
#include <time.h>

#include "alert_logic.h"
#include "command_parser.h"
#include "config.h"
#include "messages.h"
#include "price_health.h"
#include "price_source.h"
#include "secrets.h"
#include "settings_store.h"
#include "telegram_bot.h"

static AlertState state;
static PriceHealth health;
static bool hasPrice = false;
static double lastPrice = 0;
static unsigned long lastPriceAtMs = 0;
static unsigned long lastPricePollMs = 0;
static unsigned long lastBotPollMs = 0;
static unsigned long lastWifiAttemptMs = 0;
static bool firstPricePoll = true;
static bool timeSynced = false;
static bool onlineSent = false;

static bool ensureWifi(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (now - lastWifiAttemptMs >= WIFI_RETRY_MS) {
    lastWifiAttemptMs = now;
    Serial.println("[wifi] reconectando...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  return false;
}

static bool syncTime() {
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
  unsigned long start = millis();
  while (time(nullptr) < 1700000000 && millis() - start < NTP_TIMEOUT_MS) delay(200);
  bool ok = time(nullptr) >= 1700000000;
  Serial.println(ok ? "[ntp] hora sincronizada" : "[ntp] sin respuesta, reintentando");
  return ok;
}

static void checkPrice(unsigned long now) {
  double price;
  if (!fetchBtcUsd(price)) {
    Serial.println("[price] no se pudo obtener el precio");
    if (shouldSendFailAlert(health, now, PRICE_FAIL_ALERT_MS) && botSend(priceFailMsg())) {
      health.failAlertSent = true;
    }
    return;
  }

  if (health.failAlertSent) botSend(priceRecoveredMsg(price));
  health.failAlertSent = false;
  health.lastOkMs = now;
  hasPrice = true;
  lastPrice = price;
  lastPriceAtMs = now;
  Serial.printf("[price] BTC = %.2f USD\n", price);

  EvalResult r = evaluate(state, price, REARM_MARGIN);
  bool lowSent = r.lowCrossed && botSend(lowAlertMsg(state.low.value, price));
  bool highSent = r.highCrossed && botSend(highAlertMsg(state.high.value, price));
  AlertState next = commitAlerts(r, lowSent, highSent);
  if (next != state) {
    state = next;
    saveAlertState(state);
  }
}

static std::string handleCommand(const std::string& text) {
  Command cmd = parseCommand(text);
  switch (cmd.type) {
    case CommandType::SetLow:
    case CommandType::SetHigh: {
      bool isLow = cmd.type == CommandType::SetLow;
      if (!cmd.valid) return usageMsg(isLow);
      AlertState next = state;
      SetResult res = isLow ? setLow(next, cmd.value) : setHigh(next, cmd.value);
      if (res == SetResult::Conflict) {
        return conflictMsg(isLow, isLow ? state.high.value : state.low.value);
      }
      state = next;
      saveAlertState(state);
      return thresholdSetMsg(isLow, cmd.value);
    }
    case CommandType::Status: {
      StatusInfo info;
      info.hasPrice = hasPrice;
      info.price = lastPrice;
      info.priceAgeSec = (millis() - lastPriceAtMs) / 1000;
      info.state = state;
      info.rssi = WiFi.RSSI();
      info.uptimeSec = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
      return statusMsg(info);
    }
    case CommandType::Help:
    case CommandType::Unknown:
    default:
      return helpMsg();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[boot] BTC Price Alerts");

  loadAlertState(state);
  Serial.printf("[boot] low=%.2f (%s) high=%.2f (%s)\n",
                state.low.value, state.low.armed ? "armado" : "disparado",
                state.high.value, state.high.armed ? "armado" : "disparado");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs = millis();
  Serial.print("[wifi] conectando");
  while (WiFi.status() != WL_CONNECTED && millis() - lastWifiAttemptMs < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " sin conexión (se reintentará)");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }

  botBegin();
  health.lastOkMs = millis();
}

void loop() {
  unsigned long now = millis();
  if (!ensureWifi(now)) {
    delay(100);
    return;
  }
  if (!timeSynced) {
    timeSynced = syncTime();
    if (!timeSynced) return;
  }

  if (firstPricePoll || now - lastPricePollMs >= PRICE_INTERVAL_MS) {
    firstPricePoll = false;
    lastPricePollMs = now;
    checkPrice(now);
  }

  if (!onlineSent) onlineSent = botSend(onlineMsg(hasPrice, lastPrice));

  if (now - lastBotPollMs >= BOT_POLL_INTERVAL_MS) {
    lastBotPollMs = now;
    botPoll(handleCommand);
  }

  delay(10);
}

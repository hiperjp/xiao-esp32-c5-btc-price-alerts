#include "telegram_bot.h"

#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

static WiFiClientSecure client;
static UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

void botBegin() {
  client.useBuiltinCACertBundle();
}

bool botSend(const std::string& text) {
  bool ok = bot.sendMessage(TELEGRAM_CHAT_ID, text.c_str());
  if (!ok) Serial.println("[bot] error enviando mensaje");
  return ok;
}

void botPoll(CommandHandler handler) {
  int count = bot.getUpdates(bot.last_message_received + 1);
  while (count > 0) {
    for (int i = 0; i < count; ++i) {
      const telegramMessage& msg = bot.messages[i];
      if (msg.chat_id != TELEGRAM_CHAT_ID) {
        Serial.printf("[bot] mensaje ignorado del chat %s\n", msg.chat_id.c_str());
        continue;
      }
      Serial.printf("[bot] comando: %s\n", msg.text.c_str());
      botSend(handler(msg.text.c_str()));
    }
    count = bot.getUpdates(bot.last_message_received + 1);
  }
}

#pragma once
#include <string>

void botBegin();
bool botSend(const std::string& text);

using CommandHandler = std::string (*)(const std::string& text);

// Only messages from TELEGRAM_CHAT_ID reach the handler; anyone can find and
// message a public bot.
void botPoll(CommandHandler handler);

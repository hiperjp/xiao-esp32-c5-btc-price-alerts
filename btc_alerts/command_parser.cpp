#include "command_parser.h"

#include <cctype>
#include <vector>

#include "decimal.h"

static std::string toLower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

static std::vector<std::string> splitWords(const std::string& text) {
  std::vector<std::string> words;
  std::string current;
  for (char c : text) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!current.empty()) {
        words.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) words.push_back(current);
  return words;
}

Command parseCommand(const std::string& text) {
  Command cmd;
  std::vector<std::string> words = splitWords(text);
  if (words.empty() || words[0][0] != '/') return cmd;

  // Telegram appends "@BotName" to commands picked from the command menu.
  std::string name = toLower(words[0].substr(0, words[0].find('@')));
  if (name == "/status") {
    cmd.type = CommandType::Status;
    return cmd;
  }
  if (name == "/help" || name == "/start") {
    cmd.type = CommandType::Help;
    return cmd;
  }
  if (name != "/low" && name != "/high") return cmd;

  cmd.type = name == "/low" ? CommandType::SetLow : CommandType::SetHigh;
  if (words.size() != 2) {
    cmd.valid = false;
    return cmd;
  }
  if (toLower(words[1]) == "off") {
    cmd.value = 0;
    return cmd;
  }
  cmd.valid = parsePositiveDecimal(words[1], cmd.value);
  return cmd;
}

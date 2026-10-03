#pragma once
#include <string>

enum class CommandType { SetLow, SetHigh, Status, Help, Unknown };

struct Command {
  CommandType type = CommandType::Unknown;
  bool valid = true;
  double value = 0;
};

Command parseCommand(const std::string& text);

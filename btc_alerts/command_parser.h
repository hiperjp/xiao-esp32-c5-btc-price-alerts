#pragma once
#include <string>

enum class CommandType { SetLow, SetHigh, Status, Help, Unknown };

struct Command {
  CommandType type = CommandType::Unknown;
  bool valid = true;  // false si /low o /high traen un argumento inválido
  double value = 0;   // SetLow/SetHigh: precio en USD, 0 = desactivar
};

// Interpreta un mensaje de Telegram. Ignora mayúsculas en el comando, espacios
// sobrantes y el sufijo "@NombreDelBot".
Command parseCommand(const std::string& text);

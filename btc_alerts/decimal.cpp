#include "decimal.h"

#include <cmath>
#include <cstdlib>

static bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

bool parsePositiveDecimal(const std::string& text, double& out) {
  size_t i = 0;
  size_t digitsBefore = 0;
  while (i < text.size() && isDigit(text[i])) {
    ++i;
    ++digitsBefore;
  }
  if (i < text.size() && text[i] == '.') {
    ++i;
    size_t digitsAfter = 0;
    while (i < text.size() && isDigit(text[i])) {
      ++i;
      ++digitsAfter;
    }
    if (digitsAfter == 0) return false;
  }
  if (digitsBefore == 0 || i != text.size()) return false;

  double value = std::strtod(text.c_str(), nullptr);
  if (!std::isfinite(value) || value <= 0) return false;
  out = value;
  return true;
}

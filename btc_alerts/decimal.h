#pragma once
#include <string>

// Strict on purpose: inputs like "90,000" or "1e5" must be rejected rather than
// partially parsed into a different threshold than the user meant.
bool parsePositiveDecimal(const std::string& text, double& out);

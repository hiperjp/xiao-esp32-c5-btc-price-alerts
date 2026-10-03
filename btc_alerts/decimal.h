#pragma once
#include <string>

// Parsea un decimal positivo con formato estricto: dígitos con un punto decimal
// opcional seguido de dígitos ("90000", "90000.5"). Rechaza signos, comas,
// espacios, exponentes, cero y valores no finitos. Si falla, out no se modifica.
bool parsePositiveDecimal(const std::string& text, double& out);

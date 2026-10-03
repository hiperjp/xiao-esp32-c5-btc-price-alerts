#pragma once
#include <cstdint>

// Seguimiento de fallos sostenidos al obtener el precio.
struct PriceHealth {
  uint32_t lastOkMs = 0;       // millis() del último precio obtenido (o del arranque)
  bool failAlertSent = false;  // ya se avisó del fallo actual
};

// true si hay que enviar el aviso "no puedo obtener el precio": pasaron al menos
// failAfterMs desde el último precio y todavía no se avisó. La resta sin signo
// de 32 bits tolera el desborde de millis().
bool shouldSendFailAlert(const PriceHealth& h, uint32_t nowMs, uint32_t failAfterMs);

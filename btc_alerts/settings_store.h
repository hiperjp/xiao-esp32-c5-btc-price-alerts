#pragma once
#include "alert_logic.h"

// Persistencia de umbrales y estado armado/disparado en NVS.
// En el primer arranque (NVS vacío) deja los umbrales desactivados y armados.
void loadAlertState(AlertState& state);
void saveAlertState(const AlertState& state);

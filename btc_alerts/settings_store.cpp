#include "settings_store.h"

#include <Arduino.h>
#include <Preferences.h>

static const char* NVS_NAMESPACE = "btcalert";

void loadAlertState(AlertState& state) {
  Preferences prefs;
  // On first boot the namespace doesn't exist yet: begin() fails and get*() fall
  // back to the defaults, which is the intended initial state.
  prefs.begin(NVS_NAMESPACE, true);
  state.low.value = prefs.getDouble("low", 0);
  state.low.armed = prefs.getBool("lowArmed", true);
  state.high.value = prefs.getDouble("high", 0);
  state.high.armed = prefs.getBool("highArmed", true);
  prefs.end();
}

void saveAlertState(const AlertState& state) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    Serial.println("[nvs] no se pudo abrir el almacenamiento");
    return;
  }
  prefs.putDouble("low", state.low.value);
  prefs.putBool("lowArmed", state.low.armed);
  prefs.putDouble("high", state.high.value);
  prefs.putBool("highArmed", state.high.armed);
  prefs.end();
}

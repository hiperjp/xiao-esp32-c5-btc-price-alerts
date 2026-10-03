#pragma once

// Un umbral de precio en USD. value == 0 significa desactivado.
struct Threshold {
  double value = 0;
  bool armed = true;
};

struct AlertState {
  Threshold low;
  Threshold high;
};

bool operator==(const Threshold& a, const Threshold& b);
bool operator==(const AlertState& a, const AlertState& b);
bool operator!=(const AlertState& a, const AlertState& b);

// Resultado de evaluar un precio: el nuevo estado (umbrales cruzados ya
// desarmados) y qué umbrales se cruzaron en esta evaluación.
struct EvalResult {
  AlertState next;
  bool lowCrossed = false;
  bool highCrossed = false;
};

// Regla de disparo y rearme con histéresis. margin es una fracción (0.005 = 0.5 %).
EvalResult evaluate(const AlertState& state, double price, double margin);

// Estado a guardar tras intentar enviar las alertas: un umbral cuya alerta no
// se pudo enviar vuelve a quedar armado para reintentar en el siguiente ciclo.
AlertState commitAlerts(const EvalResult& result, bool lowSent, bool highSent);

enum class SetResult { Ok, Conflict };

// Fija el umbral (0 = desactivar) y lo deja armado. Devuelve Conflict, sin
// modificar el estado, si ambos umbrales quedarían activos con low >= high.
SetResult setLow(AlertState& state, double value);
SetResult setHigh(AlertState& state, double value);

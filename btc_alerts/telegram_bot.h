#pragma once
#include <string>

// Inicializa el cliente TLS del bot con el bundle de certificados raíz del core.
void botBegin();

// Envía un mensaje al chat configurado. Devuelve false si falla.
bool botSend(const std::string& text);

// Recibe el texto de un mensaje y devuelve la respuesta a enviar.
using CommandHandler = std::string (*)(const std::string& text);

// Lee los mensajes nuevos y responde con handler() solo a los del chat
// configurado; los demás se ignoran (se registran en el log serie).
void botPoll(CommandHandler handler);

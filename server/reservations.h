#pragma once

#include <string>

enum class ReserveStatus {
    ACC,   // Reserva aceite
    REJ,   // Rejeitada (não há lugares suficientes)
    CLS,   // Evento fechado pelo criador
    SLD,   // Evento esgotado
    PST,   // Evento passado
    NLG,   // User não logged in
    WRP,   // Password errada
    NOK    // Erro genérico ou evento inexistente
};

// Escreve no servidor quanto foi reservado.
// people_requested é o nº pedido pelo user.
// remaining_out devolve o nº de lugares restantes (caso REJ).
ReserveStatus es_make_reservation(const std::string &uid,
                                  const std::string &pass,
                                  const std::string &eid,
                                  int people_requested,
                                  int &remaining_out);

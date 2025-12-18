// udp_client.h
#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include "user.h"
#include <string>  

// Envia 'request' por UDP para o ES e devolve a resposta em 'response_out'.
// Retorna 0 em sucesso, -1 em erro.
int udp_send_and_receive(const ClientNetConfig *cfg,
                         const std::string &request,
                         std::string &response_out);

#endif

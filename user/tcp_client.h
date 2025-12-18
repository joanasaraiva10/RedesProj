#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include "user.h"

// abre um socket TCP ligado ao ES (usa cfg->server_ip e cfg->server_port)
int tcp_connect(const ClientNetConfig *cfg);

// envia todos os 'len' bytes (repetindo write se for preciso)
int tcp_send_all(int fd, const void *buf, size_t len);

// lê uma linha (até '\n'), devolve como string (dinâmico)
std::string tcp_recv_line(int fd);

#endif

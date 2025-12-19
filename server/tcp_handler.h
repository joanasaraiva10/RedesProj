#pragma once
#include <cstdint>

// Trata UMA ligação TCP (UM comando)
void tcp_handle_connection(int fd, bool verbose, const char *ip, uint16_t port);

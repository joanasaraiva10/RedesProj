#pragma once
#include <cstdint>

int tcp_create_listen_socket(std::uint16_t port);

void tcp_accept_and_fork(int listen_fd, int udp_fd, bool verbose);

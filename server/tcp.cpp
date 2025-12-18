// server/tcp.cpp
#include "tcp.h"
#include "tcp_handler.h"

#include <iostream>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int tcp_create_listen_socket(std::uint16_t port)
{
    struct addrinfo hints{};
    struct addrinfo *res = nullptr;

    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE | AI_NUMERICSERV;

    char port_str[16];
    std::snprintf(port_str, sizeof(port_str), "%u", port);

    int err = ::getaddrinfo(nullptr, port_str, &hints, &res);
    if (err != 0) {
        std::cerr << "TCP getaddrinfo: " << ::gai_strerror(err) << "\n";
        return -1;
    }

    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        std::perror("socket(TCP)");
        ::freeaddrinfo(res);
        return -1;
    }

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (::bind(fd, res->ai_addr, res->ai_addrlen) < 0) {
        std::perror("bind(TCP)");
        ::close(fd);
        ::freeaddrinfo(res);
        return -1;
    }

    if (::listen(fd, 32) < 0) {
        std::perror("listen(TCP)");
        ::close(fd);
        ::freeaddrinfo(res);
        return -1;
    }

    ::freeaddrinfo(res);
    return fd;
}

void tcp_accept_and_fork(int listen_fd, int udp_fd, bool verbose)
{
    struct sockaddr_in cli{};
    socklen_t len = sizeof(cli);

    int cfd = ::accept(listen_fd, (struct sockaddr*)&cli, &len);
    if (cfd < 0) {
        std::perror("accept");
        return;
    }

    if (verbose) {
        std::cout << "[ES][TCP] Accepted connection from "
                  << ::inet_ntoa(cli.sin_addr)
                  << ":" << ntohs(cli.sin_port)
                  << " (fd=" << cfd << ")\n";
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        std::perror("fork");
        ::close(cfd);
        return;
    }

    if (pid == 0) {
        // FILHO
        ::close(listen_fd);
        ::close(udp_fd);

        tcp_handle_connection(cfd);  // trata UM comando

        std::_Exit(0);
    }

    // PAI
    ::close(cfd);
}

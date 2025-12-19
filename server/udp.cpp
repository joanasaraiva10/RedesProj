// udp.cpp
#include "udp.h"

#include <sys/socket.h>
#include <unistd.h>
#include "udp.h"

#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

ssize_t udp_recv_datagram(int fd, char *buf, size_t maxlen, UdpPeer &peer)
{
    peer.addrlen = sizeof(peer.addr);
    return ::recvfrom(fd,
                      buf,
                      maxlen,
                      0,
                      reinterpret_cast<sockaddr*>(&peer.addr),
                      &peer.addrlen);
}

ssize_t udp_send_datagram(int fd, const char *buf, size_t len, const UdpPeer &peer)
{
    return ::sendto(fd,
                    buf,
                    len,
                    0,
                    reinterpret_cast<const sockaddr*>(&peer.addr),
                    peer.addrlen);
}


int udp_create_socket(std::uint16_t port)
{
    struct addrinfo hints{};
    struct addrinfo *res = nullptr;

    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags    = AI_PASSIVE | AI_NUMERICSERV;

    char port_str[16];
    std::snprintf(port_str, sizeof(port_str), "%u", port);

    int err = ::getaddrinfo(nullptr, port_str, &hints, &res);
    if (err != 0) {
        std::cerr << "udp_create_socket: getaddrinfo: "
                  << ::gai_strerror(err) << "\n";
        return -1;
    }

    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        std::perror("udp_create_socket: socket");
        ::freeaddrinfo(res);
        return -1;
    }

    int opt = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::perror("udp_create_socket: setsockopt");
    }

    if (::bind(fd, res->ai_addr, res->ai_addrlen) < 0) {
        std::perror("udp_create_socket: bind");
        ::close(fd);
        ::freeaddrinfo(res);
        return -1;
    }

    ::freeaddrinfo(res);
    return fd;
}
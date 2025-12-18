#include "tcp_client.h"

#include <cstring>
#include <string>
#include <stdexcept>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

int tcp_connect(const ClientNetConfig *cfg)
{
    int fd;
    struct addrinfo hints{}, *res = nullptr;
    int err;
    char port_str[16];

    std::snprintf(port_str, sizeof(port_str), "%d", cfg->server_port);

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP

    err = getaddrinfo(cfg->server_ip, port_str, &hints, &res);
    if (err != 0) {
        throw std::runtime_error(std::string("getaddrinfo: ") +
                                 gai_strerror(err));
    }

    fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        throw std::runtime_error("socket failed");
    }

    if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        ::close(fd);
        throw std::runtime_error("connect failed");
    }

    freeaddrinfo(res);
    return fd;
}

int tcp_send_all(int fd, const void *buf, size_t len)
{
    const char *ptr = static_cast<const char*>(buf);
    size_t nleft = len;

    while (nleft > 0) {
        ssize_t n = ::write(fd, ptr, nleft);
        if (n < 0) {
            return -1; // erro
        }
        ptr   += n;
        nleft -= n;
    }
    return 0; // sucesso
}

// lê até '\n' (ou EOF), devolve std::string
std::string tcp_recv_line(int fd)
{
    std::string line;
    char c;
    ssize_t n;

    while (true) {
        n = ::read(fd, &c, 1);
        if (n <= 0) {
            break;  // erro ou ligação fechada
        }
        line.push_back(c);
        if (c == '\n') {
            break;
        }
    }
    return line;
}

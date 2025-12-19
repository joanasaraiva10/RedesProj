// udp_client.cpp
#include "udp_client.h"

#include <vector>
#include <stdexcept>
#include <cstring>

#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <netdb.h>
#include <unistd.h>

int udp_send_and_receive(const ClientNetConfig *cfg,
                         const std::string &request,
                         std::string &response_out)
{
    int fd = -1;
    struct addrinfo hints{}, *res = nullptr;

    try {
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;       // IPv4
        hints.ai_socktype = SOCK_DGRAM;   // UDP

        char port_str[16];
        std::snprintf(port_str, sizeof(port_str), "%d", cfg->server_port);

        int err = getaddrinfo(cfg->server_ip, port_str, &hints, &res);
        if (err != 0) {
            throw std::runtime_error(gai_strerror(err));
        }

        fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0) {
            freeaddrinfo(res);
            throw std::runtime_error("socket() failed");
        }

        ssize_t n = ::sendto(fd,
                             request.data(),
                             request.size(),
                             0,
                             res->ai_addr,
                             res->ai_addrlen);

        freeaddrinfo(res);

        if (n < 0 || static_cast<size_t>(n) != request.size()) {
            ::close(fd);
            throw std::runtime_error("sendto() failed");
        }

        // Buffer para receber com limite máximo do UDP (~64KB)
        std::vector<char> buf(65535);

        struct sockaddr_storage src_addr{};
        socklen_t addrlen = sizeof(src_addr);

        n = ::recvfrom(fd,
                       buf.data(),
                       buf.size(),
                       0,
                       reinterpret_cast<struct sockaddr*>(&src_addr),
                       &addrlen);
        if (n < 0) {
            ::close(fd);
            throw std::runtime_error("recvfrom() failed");
        }

        ::close(fd);
        fd = -1;

        response_out.assign(buf.data(), static_cast<size_t>(n));
        return 0;
    }
    catch (...) {
        if (fd >= 0) ::close(fd);
        return -1;
    }
}

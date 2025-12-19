// server/main.cpp
#include <iostream>
#include <csignal>
#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "parser.h"
#include "udp_handler.h"
#include "tcp.h"
#include "udp.h"

// sockets globais para os handlers de sinal
static int  g_udp_sock = -1;
static int  g_tcp_sock = -1;
static bool g_verbose  = false;

//Signals

static void sigint_handler(int)
{
    std::cout << "\n[ES] SIGINT received: closing sockets.\n";

    if (g_udp_sock != -1) {
        ::close(g_udp_sock);
        g_udp_sock = -1;
    }
    if (g_tcp_sock != -1) {
        ::close(g_tcp_sock);
        g_tcp_sock = -1;
    }

    std::_Exit(0);
}

static void sigchld_handler(int)
{
    // limpar processos filho (forks TCP)
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}
}

static void setup_signals()
{
    struct sigaction sa{};
    sa.sa_handler = sigint_handler;
    ::sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ::sigaction(SIGINT, &sa, nullptr);

    struct sigaction sa2{};
    sa2.sa_handler = sigchld_handler;
    ::sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = SA_RESTART;
    ::sigaction(SIGCHLD, &sa2, nullptr);
}


int main(int argc, char **argv)
{
    ServerConfig cfg{}; 
    parse_server_args(cfg, argc, argv);
    g_verbose = cfg.verbose;

    setup_signals();

    // cria sockets
    g_udp_sock = udp_create_socket(cfg.port);
    g_tcp_sock = tcp_create_listen_socket(cfg.port);

    if (g_udp_sock < 0 || g_tcp_sock < 0) {
        std::cerr << "Error creating sockets.\n";
        return 1;
    }

    std::cout << "[ES] Listening on TCP/UDP port " << cfg.port << "\n";
    if (g_verbose) {
        std::cout << "[ES] Verbose ON\n";
    }

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_udp_sock, &readfds);
        FD_SET(g_tcp_sock, &readfds);

        int maxfd = (g_udp_sock > g_tcp_sock) ? g_udp_sock : g_tcp_sock;

        int ready = ::select(maxfd + 1, &readfds, nullptr, nullptr, nullptr);
        if (ready < 0) {
            if (errno == EINTR) continue;
            std::perror("select");
            break;
        }

        // UDP pronto
        if (FD_ISSET(g_udp_sock, &readfds)) {
            udp_handle_datagram(g_udp_sock, g_verbose);
        }

        // Nova ligação TCP pronta
        if (FD_ISSET(g_tcp_sock, &readfds)) {
            tcp_accept_and_fork(g_tcp_sock, g_udp_sock, g_verbose);
        }
    }

    if (g_udp_sock != -1) ::close(g_udp_sock);
    if (g_tcp_sock != -1) ::close(g_tcp_sock);
    return 0;
}

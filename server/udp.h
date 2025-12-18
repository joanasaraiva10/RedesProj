// udp.h
#ifndef ES_UDP_H
#define ES_UDP_H

#include <cstddef>
#include <netinet/in.h>
#include <cstdint>

// Informação sobre o peer que enviou o datagrama
struct UdpPeer {
    sockaddr_in addr;
    socklen_t   addrlen;
};

// Cria e devolve o socket UDP já bindado ao porto.
// Retorna -1 em caso de erro.
int udp_create_socket(std::uint16_t port);

// Lê um datagrama do socket UDP.
// Retorna nº de bytes lidos ou -1 em erro.
ssize_t udp_recv_datagram(int fd, char *buf, size_t maxlen, UdpPeer &peer);

// Envia um datagrama de resposta para o peer.
// Retorna nº de bytes enviados ou -1 em erro.
ssize_t udp_send_datagram(int fd, const char *buf, size_t len, const UdpPeer &peer);

#endif

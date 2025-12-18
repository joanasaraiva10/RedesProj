#ifndef ES_UDP_HANDLER_H
#define ES_UDP_HANDLER_H

// Processa um único datagrama UDP recebido em udp_fd.
// Se verbose==true, imprime info sobre o pedido.
void udp_handle_datagram(int udp_fd, bool verbose);

#endif
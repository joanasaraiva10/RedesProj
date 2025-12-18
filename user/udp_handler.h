#ifndef UDP_HANDLER_H
#define UDP_HANDLER_H

#include "user.h"



// Dispatcher UDP: descobre o comando a partir da linha e chama o handler certo
void udp_dispatch_command(ClientState *state,
                          const ClientNetConfig *cfg,
                          const char *line);

#endif

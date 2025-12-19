#ifndef UDP_HANDLER_H
#define UDP_HANDLER_H

#include "user.h"



// Dispatcher UDP
void udp_dispatch_command(ClientState *state,
                          const ClientNetConfig *cfg,
                          const char *line);

#endif

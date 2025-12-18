#ifndef TCP_HANDLER_H
#define TCP_HANDLER_H

#include "user.h"

void tcp_dispatch_command(ClientState *state,
                          const ClientNetConfig *cfg,
                          const char *line);

#endif
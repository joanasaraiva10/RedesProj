#ifndef PARSER_H
#define PARSER_H

#include <cstddef>   // size_t
#include "user.h"

#define GN 5


// Tipos de comandos que o utilizador pode escrever.
typedef enum {
    CMD_LOGIN,
    CMD_LOGOUT,
    CMD_UNREGISTER,
    CMD_MYEVENTS,
    CMD_MYRES,
    CMD_CREATE,
    CMD_CLOSE,
    CMD_LIST,
    CMD_SHOW,
    CMD_RESERVE,
    CMD_CHANGEPASS,
    CMD_INVALID
} UserCommandType;


//Tipo de protocolo a usar para cada comando.
typedef enum {
    PROTO_UDP,
    PROTO_TCP,
    PROTO_LOCAL,
    PROTO_INVALID
} ProtocolKind;


//Mapeia a primeira palavra do input para um UserCommandType.
UserCommandType command_from_word(const char *word);

//Dado um comando (CMD_LOGIN, CMD_LIST,...),
//devolve o protocolo adequado.
ProtocolKind command_protocol(UserCommandType cmd);

//Faz parse ao comando "login <uid> <password>".
int parse_login_command(const char *line,
                        char *uid_out,  size_t uid_sz,
                        char *pass_out, size_t pass_sz);

// Lê argumentos da linha de comandos:
void parse_args(ClientNetConfig *cfg, int argc, char **argv);

//lê comandos do terminal e despacha para UDP/TCP.
void user_loop(ClientState *state, const ClientNetConfig *cfg);

#endif // PARSER_H

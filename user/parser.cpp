#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "user.h"
#include "parser.h"
#include "udp_handler.h"
#include "tcp_handler.h"

/* =====================
 *  Mapear palavra → comando
 * ===================== */

UserCommandType command_from_word(const char *word) {
    if (strcmp(word, "login") == 0)           return CMD_LOGIN;
    if (strcmp(word, "logout") == 0)          return CMD_LOGOUT;
    if (strcmp(word, "unregister") == 0)      return CMD_UNREGISTER;
    if (strcmp(word, "myevents") == 0 ||
        strcmp(word, "mye") == 0)             return CMD_MYEVENTS;
    if (strcmp(word, "myres") == 0 ||
        strcmp(word, "myr") == 0 ||
        strcmp(word, "myreservations") == 0)  return CMD_MYRES;
    if (strcmp(word, "create") == 0)          return CMD_CREATE;
    if (strcmp(word, "close") == 0)           return CMD_CLOSE;
    if (strcmp(word, "list") == 0)            return CMD_LIST;
    if (strcmp(word, "show") == 0)            return CMD_SHOW;
    if (strcmp(word, "reserve") == 0)         return CMD_RESERVE;
    if (strcmp(word, "changepw") == 0 ||
        strcmp(word, "changePass") == 0)      return CMD_CHANGEPASS;

    return CMD_INVALID;
}

/* =====================
 *  Comando → tipo de protocolo
 * ===================== */

ProtocolKind command_protocol(UserCommandType cmd) {
    switch (cmd) {
        /* UDP  */
        case CMD_LOGIN:
        case CMD_LOGOUT:
        case CMD_UNREGISTER:
        case CMD_MYEVENTS:
        case CMD_MYRES:
            return PROTO_UDP;

        /* TCP  */
        case CMD_CREATE:
        case CMD_CLOSE:
        case CMD_LIST:
        case CMD_SHOW:
        case CMD_RESERVE:
        case CMD_CHANGEPASS:
            return PROTO_TCP;

        case CMD_INVALID:
        default:
            return PROTO_INVALID;
    }
}

void parse_args(ClientNetConfig *cfg, int argc, char **argv) {
    /* valores por omissão */
    strcpy(cfg->server_ip, "127.0.0.1"); // ES na mesma máquina
    cfg->server_port = 58000;            // aqui podes somar o nº de grupo

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) {
            strncpy(cfg->server_ip, argv[++i], sizeof(cfg->server_ip) - 1);
            cfg->server_ip[sizeof(cfg->server_ip) - 1] = '\0';
        } else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
            cfg->server_port = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Usage: %s [-n ESIP] [-p ESport]\n", argv[0]);
            exit(1);
        }
    }
}

/* =====================
 *  Loop principal do cliente
 * ===================== */

void user_loop(ClientState *state, const ClientNetConfig *cfg)
{
    char line[256];

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            /* EOF → sair */
            break;
        }

        /* remover \n */
        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0') {
            continue;  // linha vazia
        }

        /* comando local "exit" */
        if (!strcmp(line, "exit") || !strcmp(line, "quit")) {
            if (state->logged_in) {
                printf("You are still logged in as %s. "
                    "Please logout first.\n", state->uid.c_str());
            continue;
            }
            printf("Exiting...\n");
            break;
        }

        /* copiar linha para analisar a primeira palavra */
        char buffer[256];
        strncpy(buffer, line, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        char *first = strtok(buffer, " ");
        if (!first)
            continue;

        UserCommandType cmd = command_from_word(first);
        ProtocolKind proto = command_protocol(cmd);

        if (proto == PROTO_INVALID) {
            fprintf(stderr, "Unknown command: %s\n", first);
            continue;
        }

        /* ======================
         *      SOMENTE UDP
         * ====================== */

        if (proto == PROTO_UDP) {
            udp_dispatch_command(state, cfg, line);
        } 
        else {
           tcp_dispatch_command(state, cfg, line);
        }
    }
}


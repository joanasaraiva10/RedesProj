#ifndef PARSER_H
#define PARSER_H

#include <cstdint>

#define GN 5

struct ServerConfig {
    bool        verbose;
    std::uint16_t port;   // porto ES (TCP+UDP)
};

// Lê argc/argv, aplica defaults e valida.
// Em caso de erro, imprime usage e termina o programa.
void parse_server_args(ServerConfig &cfg, int argc, char **argv);

#endif

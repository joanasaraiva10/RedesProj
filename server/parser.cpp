#include "parser.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <getopt.h>

void parse_server_args(ServerConfig &cfg, int argc, char **argv)
{
    // defaults
    cfg.verbose = false;
    cfg.port    = 58000 + GN;  

    int opt;
    while ((opt = ::getopt(argc, argv, "vp:")) != -1) {
        switch (opt) {
        case 'v':
            cfg.verbose = true;
            break;

        case 'p': {
            int p = std::atoi(optarg);
            if (p <= 0 || p > 65535) {
                std::cerr << "Invalid port: " << optarg << "\n";
                std::exit(EXIT_FAILURE);
            }
            cfg.port = static_cast<std::uint16_t>(p);
            break;
        }

        default:
            std::cerr << "Usage: " << argv[0] << " [-v] [-p ESport]\n";
            std::exit(EXIT_FAILURE);
        }
    }
}

#include <iostream>
#include "user.h"
#include "parser.h"
#include <signal.h>


int main(int argc, char **argv)
{
    // evita crash quando o servidor fecha a socket (EPIPE)
    signal(SIGPIPE, SIG_IGN);

    ClientNetConfig cfg{};
    ClientState state;         // uid/pass começam vazios, logged_in = false

    state.logged_in = false;   // redundante mas explícito

    parse_args(&cfg, argc, argv);
    user_loop(&state, &cfg);
    return 0;
}

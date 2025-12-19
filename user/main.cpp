#include <iostream>
#include "user.h"
#include "parser.h"

int main(int argc, char **argv) {
    ClientNetConfig cfg{};
    ClientState state;         // uid/pass começam vazios, logged_in = false

    state.logged_in = false;  

    parse_args(&cfg, argc, argv);
    user_loop(&state, &cfg);
    return 0;
}

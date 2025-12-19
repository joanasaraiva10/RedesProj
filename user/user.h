#ifndef USER_H
#define USER_H

#include <string>  

//Configuração de redes do cliente
typedef struct {
    char server_ip[64];  
    int  server_port;   
} ClientNetConfig;

//Estado lógico do cliente 
struct ClientState {
    bool logged_in = false;
    std::string uid;
    std::string pass;
};


#endif // USER_H

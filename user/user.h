#ifndef USER_H
#define USER_H

#include <string>  

/**
 * Configuração de rede do cliente:
 * - IP/hostname do ES
 * - porto do ES
 */
typedef struct {
    char server_ip[64];  
    int  server_port;   
} ClientNetConfig;

/**
 * Estado lógico do cliente:
 * - se está autenticado
 * - UID e password que usou no login
 */
struct ClientState {
    bool logged_in = false;
    std::string uid;
    std::string pass;
};


#endif // USER_H

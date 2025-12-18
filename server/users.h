#ifndef ES_USERS_H
#define ES_USERS_H

#include <string>

// Estados genéricos para operações sobre utilizadores.
// Mapeiam diretamente para as strings do protocolo.
enum class UserStatus {
    OK,
    REG,
    NOK,
    UNR,
    WRP,
    ERR,
    NID,
    NLG
};

// Converte UserStatus para a string usada no protocolo ("OK", "REG", etc.)
std::string user_status_to_string(UserStatus st);

// Helpers de estado simples
bool es_user_exists(const std::string &uid);
bool es_user_is_logged_in(const std::string &uid);

// Operações principais (seguem o enunciado e o guia):

// LIN UID password  -> RLI status (OK/REG/NOK/ERR)
UserStatus es_user_login(const std::string &uid,
                         const std::string &password);

// LOU UID password  -> RLO status (OK/NOK/UNR/WRP/ERR)
UserStatus es_user_logout(const std::string &uid,
                          const std::string &password);

// UNR UID password  -> RUR status (OK/NOK/UNR/WRP/ERR)
UserStatus es_user_unregister(const std::string &uid,
                              const std::string &password);
                              
// CPS UID oldPass newPass -> RCP status (OK/NLG/NOK/NID/ERR)
UserStatus es_user_change_password(const std::string &uid,
                                   const std::string &old_pass,
                                   const std::string &new_pass);

// Verifica se a password está correta para o utilizador
bool es_user_check_password(const std::string &uid,
                            const std::string &password);
#endif // ES_USERS_H

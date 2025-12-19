#include "users.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include "protocol.h"

namespace fs = std::filesystem;

// Diretoria base da BD de utilizadores 
static const char *USERS_DIR = "USERS";


// Helpers internos
// USERS/UID
static fs::path user_dir(const std::string &uid)
{
    return fs::path(USERS_DIR) / uid;
}

// USERS/UID/UIDpass.txt
static fs::path pass_file(const std::string &uid)
{
    return user_dir(uid) / (uid + "pass.txt");
}

// USERS/UID/UIDlogin.txt
static fs::path login_file(const std::string &uid)
{
    return user_dir(uid) / (uid + "login.txt");
}

// USERS/UID/CREATED
static fs::path created_dir(const std::string &uid)
{
    return user_dir(uid) / "CREATED";
}

// USERS/UID/RESERVED
static fs::path reserved_dir(const std::string &uid)
{
    return user_dir(uid) / "RESERVED";
}

// Lê password de pass.txt (string vazia em caso de erro)
static std::string load_password(const std::string &uid)
{
    std::ifstream in(pass_file(uid));
    if (!in.is_open()) return {};
    std::string pass;
    std::getline(in, pass);
    return pass;
}

// Cria USERS/ se não existir 
static void ensure_users_root()
{
    std::error_code ec;
    if (!fs::exists(USERS_DIR, ec)) {
        fs::create_directory(USERS_DIR, ec);
    }
}


std::string user_status_to_string(UserStatus st)
{
    switch (st) {
    case UserStatus::OK:  return "OK";
    case UserStatus::REG: return "REG";
    case UserStatus::NOK: return "NOK";
    case UserStatus::UNR: return "UNR";
    case UserStatus::WRP: return "WRP";
    case UserStatus::ERR: return "ERR";
    case UserStatus::NID: return "NID";
    case UserStatus::NLG: return "NLG";
    }
    return "ERR";
}

bool es_user_exists(const std::string &uid)
{
    if (!proto_valid_uid(uid)) return false;
    std::error_code ec;
    return fs::exists(user_dir(uid), ec) && fs::exists(pass_file(uid), ec);
}

bool es_user_is_logged_in(const std::string &uid)
{
    if (!proto_valid_uid(uid)) return false;
    std::error_code ec;
    return fs::exists(login_file(uid), ec);
}


// LIN UID password  - RLI 
//  - se UID não existir registado, cria diretoria, pass.txt, login.txt - REG
//  - se existir registado:
//        pass correta, marca login (login.txt) 
//        pass errada, NOK
UserStatus es_user_login(const std::string &uid,
                         const std::string &password)
{
    if (!proto_valid_uid(uid) || !proto_valid_password(password)) return UserStatus::ERR;

    ensure_users_root();
    std::error_code ec;

    fs::path udir = user_dir(uid);

    bool dir_exists   = fs::exists(udir, ec);
    bool pass_exists  = fs::exists(pass_file(uid), ec);

    // 1: diretoria de utilizador não existe: novo registo
    if (!dir_exists) {
        // criar USERS/UID, CREATED, RESERVED
        try {
            fs::create_directory(udir, ec);
            if (ec) return UserStatus::ERR;

            fs::create_directory(created_dir(uid), ec);
            if (ec) return UserStatus::ERR;

            fs::create_directory(reserved_dir(uid), ec);
            if (ec) return UserStatus::ERR;
        }
        catch (...) {
            return UserStatus::ERR;
        }

        // criar pass.txt
        {
            std::ofstream out(pass_file(uid));
            if (!out.is_open()) return UserStatus::ERR;
            out << password << "\n";
        }

        // criar login.txt 
        {
            std::ofstream out(login_file(uid));
            if (!out.is_open()) return UserStatus::ERR;
            out << "Logged in\n";
        }

        return UserStatus::REG;
    }

    // 2: diretoria já existe, mas pass.txt não existe
    // (utilizador já teve conta e fez unregister: herda CREATED/RESERVED)
    if (!pass_exists) {
        // criar novo pass.txt
        {
            std::ofstream out(pass_file(uid));
            if (!out.is_open()) return UserStatus::ERR;
            out << password << "\n";
        }

        // criar login.txt
        {
            std::ofstream out(login_file(uid));
            if (!out.is_open()) return UserStatus::ERR;
            out << "Logged in\n";
        }

        return UserStatus::REG;
    }

    //3: utilizador já registado, verifica password
    std::string saved = load_password(uid);
    if (saved.empty()) {
        // erro ao ler pass.txt
        return UserStatus::ERR;
    }

    if (saved != password) {
        // password errada 
        return UserStatus::NOK;
    }

    // password correta: garantir login.txt 
    {
        std::ofstream out(login_file(uid));
        if (!out.is_open()) return UserStatus::ERR;
        out << "Logged in\n";
    }

    return UserStatus::OK;
}

// LOU UID password -> RLO status
//  - se utilizador não registado → UNR
//  - se pass errada → WRP
//  - se registado mas não logged in → NOK
//  - se registado e logged in → apaga login.txt → OK

UserStatus es_user_logout(const std::string &uid,
                          const std::string &password)
{
    if (!proto_valid_uid(uid)) return UserStatus::ERR;

    std::error_code ec;

    if (!fs::exists(user_dir(uid), ec) || !fs::exists(pass_file(uid), ec)) {
        // não há registo do utilizador
        return UserStatus::UNR;
    }

    std::string saved = load_password(uid);
    if (saved.empty()) return UserStatus::ERR;

    if (saved != password) {
        return UserStatus::WRP;
    }

    fs::path lfile = login_file(uid);
    if (!fs::exists(lfile, ec)) {
        // não estava logged in
        return UserStatus::NOK;
    }

    // apaga login.txt
    fs::remove(lfile, ec);
    if (ec) return UserStatus::ERR;
    
    return UserStatus::OK;
}


// UNR UID password -> RUR status
//  - se utilizador não registado → UNR
//  - se pass errada → WRP
//  - se registado mas não logged in → NOK
//  - se registado & logged in & pass ok → apaga só pass.txt e login.txt → OK 
UserStatus es_user_unregister(const std::string &uid,
                              const std::string &password)
{
    if (!proto_valid_uid(uid)) return UserStatus::ERR;

    std::error_code ec;

    if (!fs::exists(user_dir(uid), ec) || !fs::exists(pass_file(uid), ec)) {
        return UserStatus::UNR;
    }

    std::string saved = load_password(uid);
    if (saved.empty()) return UserStatus::ERR;

    if (saved != password) {
        return UserStatus::WRP;
    }

    // tem de estar logged in, senão NOK
    fs::path lfile = login_file(uid);
    if (!fs::exists(lfile, ec)) {
        return UserStatus::NOK;
    }

    // apaga pass.txt e login.txt, mas deixa CREATED/RESERVED intactos
    fs::remove(pass_file(uid), ec);
    if (ec) return UserStatus::ERR;
    fs::remove(lfile, ec);
    if (ec) return UserStatus::ERR;

    return UserStatus::OK;
}

bool es_user_check_password(const std::string &uid,
                            const std::string &password)
{
    if (!proto_valid_uid(uid)) return false;

    std::error_code ec;
    if (!fs::exists(user_dir(uid), ec) || !fs::exists(pass_file(uid), ec)) {
        return false;
    }

    std::string saved = load_password(uid);
    if (saved.empty()) return false;

    return (saved == password);
}



// CPS UID oldPass newPass -> RCP status
//   - se user não existe → NID
//   - se não está logged in → NLG
//   - se oldPass != pass.txt → NOK
//   - se tudo ok, escreve newPass em pass.txt → OK 
UserStatus es_user_change_password(const std::string &uid,
                                   const std::string &old_pass,
                                   const std::string &new_pass)
{
    if (!proto_valid_uid(uid) || !proto_valid_password(old_pass) || !proto_valid_password(new_pass)) return UserStatus::ERR;

    std::error_code ec;

    if (!fs::exists(user_dir(uid), ec) || !fs::exists(pass_file(uid), ec)) {
        // utilizador não existe
        return UserStatus::NID;
    }

    if (!fs::exists(login_file(uid), ec)) {
        // não está logged in
        return UserStatus::NLG;
    }

    std::string saved = load_password(uid);
    if (saved.empty()) return UserStatus::ERR;

    if (saved != old_pass) {
        // password antiga incorreta
        return UserStatus::NOK;
    }

    // escrever nova password
    {
        std::ofstream out(pass_file(uid));
        if (!out.is_open()) return UserStatus::ERR;
        out << new_pass << "\n";
    }

    return UserStatus::OK;
}

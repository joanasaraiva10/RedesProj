// tcp_handler.cpp
#include "tcp_handler.h"
#include "tcp_client.h"
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h> 
#include <fstream>   // para ficheiros
#include <vector>    
#include <stdexcept> 
 

// Lê exatamente len bytes de fd, ou falha
static bool tcp_recv_exact(int fd, void *buf, size_t len)
{
    char *p = static_cast<char*>(buf);
    size_t nleft = len;

    while (nleft > 0) {
        ssize_t n = ::read(fd, p, nleft);
        if (n <= 0) {
            return false; // erro ou EOF prematuro
        }
        p     += n;
        nleft -= n;
    }
    return true;
}


static void handle_list(ClientState * /*state*/,
                        const ClientNetConfig *cfg,
                        const char *line)
{
    // 1) opcional: validar que o comando era mesmo "list"
    {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd != "list") {
            std::cerr << "Usage: list\n";
            return;
        }
    }

    int fd = -1;

    try {
        // 2) abrir ligação TCP
        fd = tcp_connect(cfg);

        // 3) enviar "LST\n"
        std::string request = "LST\n";
        if (tcp_send_all(fd, request.data(), request.size()) < 0) {
            std::cerr << "Error sending LST request.\n";
            ::close(fd);
            return;
        }

        // 4) ler uma linha de resposta
        std::string response = tcp_recv_line(fd);
        ::close(fd);
        fd = -1;

        if (response.empty()) {
            std::cerr << "Empty response to LST.\n";
            return;
        }

        // 5) parse da resposta
        std::istringstream iss(response);
        std::string tag, status;
        iss >> tag >> status;

        if (tag != "RLS") {
            std::cerr << "Protocol error on list: expected 'RLS', got '"
                      << tag << "'\n";
            return;
        }

        if (status == "NOK") {
            std::cout << "No events have been created yet.\n";
            return;
        }

        if (status != "OK") {
            std::cout << "Server returned status " << status
                      << " for list.\n";
            return;
        }

        // 6) status == OK → ler [EID name state date time] repetidamente
        std::cout << "Event list\n";

        std::string eid, name, state_str, date, time;
        int count = 0;

        while (iss >> eid >> name >> state_str >> date >> time) {
            int s = -1;
            try {
                s = std::stoi(state_str);
            } catch (...) {
                s = -1;
            }

            std::string state_text;
            switch (s) {
                case 0: state_text = "Past";     break;
                case 1: state_text = "Open";     break;
                case 2: state_text = "Sold out"; break;
                case 3: state_text = "Closed";   break;
                default: state_text = "Unknown"; break;
            }

            std::cout << "  Event " << eid
                      << " [" << name << "] on " << date
                      << " at " << time
                      << " -> " << state_text << "\n";

            ++count;
        }

        if (count == 0) {
            std::cout << "No events returned by server.\n";
        }

    } catch (const std::exception &e) {
        if (fd >= 0) ::close(fd);
        std::cerr << "LIST TCP error: " << e.what() << "\n";
    }
}



static void handle_close(ClientState *state,
                         const ClientNetConfig *cfg,
                         const char *line)
{
    // 1) tem de estar logged in
    if (!state->logged_in) {
        std::cout << "You must be logged in to close an event.\n";
        return;
    }

    // 2) parse da linha do utilizador: "close 003"
    std::string cmd, eid;
    {
        std::istringstream iss(line);
        iss >> cmd >> eid;
    }

    if (cmd != "close" || eid.empty()) {
        std::cerr << "Usage: close <EID>\n";
        return;
    }

    int fd = -1;

    try {
        // 3) abrir ligação TCP ao ES
        fd = tcp_connect(cfg);

        // 4) construir pedido CLS
        std::string request = "CLS " + state->uid + " " + state->pass +
                              " " + eid + "\n";

        if (tcp_send_all(fd, request.data(), request.size()) < 0) {
            std::cerr << "Error sending CLS request.\n";
            ::close(fd);
            return;
        }

        // 5) ler 1 linha de resposta
        std::string response = tcp_recv_line(fd);
        ::close(fd);
        fd = -1;

        if (response.empty()) {
            std::cerr << "Empty response to CLS.\n";
            return;
        }

        // 6) parse da resposta: "RCL status\n"
        std::istringstream iss(response);
        std::string tag, status;
        iss >> tag >> status;

        if (tag != "RCL" || status.empty()) {
            std::cerr << "Protocol error on close: " << response << "\n";
            return;
        }

        // 7) interpretar status
        if (status == "OK") {
            std::cout << "Event " << eid << " closed successfully.\n";
        }
        else if (status == "NLG") {
            std::cout << "User not logged in (server side).\n";
            state->logged_in = false;
        }
        else if (status == "NOE") {
            std::cout << "Event " << eid << " does not exist.\n";
        }
        else if (status == "EOW") {
            std::cout << "You are not the owner of event " << eid << ".\n";
        }
        else if (status == "SLD") {
            std::cout << "Event " << eid << " is already sold out.\n";
        }
        else if (status == "PST") {
            std::cout << "Event " << eid << " is already in the past.\n";
        }
        else if (status == "CLO") {
            std::cout << "Event " << eid << " was already closed.\n";
        }
        else if (status == "NOK") {
            std::cout << "Close failed (NOK: user does not exist or password incorrect).\n";
        }
        else if (status == "WRP") {
            std::cout << "Wrong password.\n";
        }
        else if (status == "ERR") {
            std::cout << "Error processing close request.\n";
        }
        else {
            std::cout << "Unknown status from server in close: " << status << "\n";
        }

    } catch (const std::exception &e) {
        if (fd >= 0) ::close(fd);
        std::cerr << "CLOSE TCP error: " << e.what() << "\n";
    }
}


static void handle_changepass(ClientState *state,
                              const ClientNetConfig *cfg,
                              const char *line)
{
    if (!state->logged_in) {
        std::cout << "You must be logged in to change password.\n";
        return;
    }

    // parse da linha: "changepw old new" ou "changePass old new"
    std::string cmd, oldpw, newpw;
    {
        std::istringstream iss(line);
        iss >> cmd >> oldpw >> newpw;
    }

    if ((cmd != "changepw" && cmd != "changePass") ||
        oldpw.empty() || newpw.empty()) {
        std::cerr << "Usage: changepw <oldPassword> <newPassword>\n";
        return;
    }

    int fd = -1;

    try {
        // abrir TCP
        fd = tcp_connect(cfg);

        // construir pedido CPS
        std::string request = "CPS " + state->uid + " " +
                              oldpw + " " + newpw + "\n";

        if (tcp_send_all(fd, request.data(), request.size()) < 0) {
            std::cerr << "Error sending CPS request.\n";
            ::close(fd);
            return;
        }

        // ler 1 linha de resposta
        std::string response = tcp_recv_line(fd);
        ::close(fd);
        fd = -1;

        if (response.empty()) {
            std::cerr << "Empty response to CPS.\n";
            return;
        }

        // parse: "RCP status\n"
        std::istringstream iss(response);
        std::string tag, status;
        iss >> tag >> status;

        if (tag != "RCP" || status.empty()) {
            std::cerr << "Protocol error on changePass: " << response << "\n";
            return;
        }

        if (status == "OK") {
            std::cout << "Password changed successfully.\n";
            // atualizar password no estado local
            state->pass = newpw;
        }
        else if (status == "NLG") {
            std::cout << "User not logged in (server side).\n";
            state->logged_in = false;
        }
        else if (status == "NID") {
            std::cout << "User does not exist.\n";
        }
        else if (status == "NOK") {
            std::cout << "Old password incorrect.\n";
        }
        else if (status == "ERR") {
            std::cout << "Error processing changePass request.\n";
        }
        else {
            std::cout << "Unknown status in changePass reply: " << status << "\n";
        }

    } catch (const std::exception &e) {
        if (fd >= 0) ::close(fd);
        std::cerr << "CHANGEPASS TCP error: " << e.what() << "\n";
    }
}



static void handle_reserve(ClientState *state,
                           const ClientNetConfig *cfg,
                           const char *line)
{
    // 1) precisa de estar logged in
    if (!state->logged_in) {
        std::cout << "You must be logged in to make a reservation.\n";
        return;
    }

    // 2) parse da linha do utilizador: "reserve 002 10"
    std::string cmd, eid, seats_str;
    {
        std::istringstream iss(line);
        iss >> cmd >> eid >> seats_str;
    }

    if (cmd != "reserve" || eid.empty() || seats_str.empty()) {
        std::cerr << "Usage: reserve <EID> <number_of_seats>\n";
        return;
    }

    // opcional: validar que seats é número > 0
    int seats = 0;
    try {
        seats = std::stoi(seats_str);
    } catch (...) {
        seats = 0;
    }
    if (seats <= 0) {
        std::cerr << "Number of seats must be a positive integer.\n";
        return;
    }

    int fd = -1;

    try {
        // 3) abrir ligação TCP
        fd = tcp_connect(cfg);

        // 4) construir pedido RID
        std::string request = "RID " + state->uid + " " + state->pass +
                              " " + eid + " " + seats_str + "\n";

        if (tcp_send_all(fd, request.data(), request.size()) < 0) {
            std::cerr << "Error sending RID request.\n";
            ::close(fd);
            return;
        }

        // 5) ler 1 linha de resposta
        std::string response = tcp_recv_line(fd);
        ::close(fd);
        fd = -1;

        if (response.empty()) {
            std::cerr << "Empty response to RID.\n";
            return;
        }

        // 6) parse: "RRI status [nseats]\n"
        std::istringstream iss(response);
        std::string tag, status;
        iss >> tag >> status;

        if (tag != "RRI" || status.empty()) {
            std::cerr << "Protocol error on reserve: " << response << "\n";
            return;
        }

        // 7) interpretar status
        if (status == "ACC") {
            std::cout << "Reservation successful: "
                      << seats << " seats in event " << eid << ".\n";
        }
        else if (status == "REJ") {
            // servidor envia nº de lugares ainda disponíveis
            std::string left_str;
            iss >> left_str;
            int left = 0;
            try {
                left = std::stoi(left_str);
            } catch (...) {
                left = 0;
            }

            std::cout << "Reservation rejected: not enough seats.\n";
            if (left > 0) {
                std::cout << "Remaining seats: " << left << ".\n";
            }
        }
        else if (status == "SLD") {
            std::cout << "Event " << eid << " is sold out.\n";
        }
        else if (status == "CLS") {
            std::cout << "Event " << eid << " is closed.\n";
        }
        else if (status == "PST") {
            std::cout << "Event " << eid << " is in the past.\n";
        }
        else if (status == "NOE") {
            std::cout << "Event " << eid << " does not exist.\n";
        }
        else if (status == "NLG") {
            std::cout << "User not logged in (server side).\n";
            state->logged_in = false;
        }
        else if (status == "WRP") {
            std::cout << "Wrong password.\n";
        }
        else if (status == "NOK") {
            std::cout << "Reservation failed (NOK).\n";
        }
        else if (status == "ERR") {
            std::cout << "Error processing reservation request.\n";
        }
        else {
            std::cout << "Unknown status in reserve reply: " << status << "\n";
        }

    } catch (const std::exception &e) {
        if (fd >= 0) ::close(fd);
        std::cerr << "RESERVE TCP error: " << e.what() << "\n";
    }
}


static void handle_create(ClientState *state,
                          const ClientNetConfig *cfg,
                          const char *line)
{
    if (!state->logged_in) {
        std::cout << "You must be logged in to create an event.\n";
        return;
    }

    std::string cmd, name, fname, date, time, attendees_str;

    {
        std::istringstream iss(line);
        iss >> cmd >> name >> fname >> date >> time >> attendees_str;
    }

    if (cmd != "create" || name.empty() || fname.empty() ||
        date.empty() || time.empty() || attendees_str.empty()) {
        std::cerr << "Usage: create <name> <event_fname> <dd-mm-yyyy> <hh:mm> <num_attendees>\n";
        return;
    }

    int attendees = 0;
    try {
        attendees = std::stoi(attendees_str);
    } catch (...) {
        attendees = 0;
    }

    if (attendees < 10 || attendees > 999) {
        std::cerr << "Number of attendees must be between 10 and 999.\n";
        return;
    }

    // Ler ficheiro local (event_fname)
    std::ifstream fin(fname, std::ios::binary);
    if (!fin.is_open()) {
        std::cerr << "Could not open file \"" << fname << "\".\n";
        return;
    }

    fin.seekg(0, std::ios::end);
    std::streamoff sz = fin.tellg();
    if (sz < 0) {
        std::cerr << "Could not determine file size for \"" << fname << "\".\n";
        return;
    }

    if (sz > 10'000'000) { // 10 MB limite do enunciado
        std::cerr << "File is too large (max 10 MB).\n";
        return;
    }

    fin.seekg(0, std::ios::beg);
    std::string file_data;
    file_data.resize(static_cast<size_t>(sz));
    if (sz > 0) {
        fin.read(file_data.data(), sz);
        if (!fin.good()) {
            std::cerr << "Error reading file \"" << fname << "\".\n";
            return;
        }
    }

    int fd = -1;

    try {
        // Abrir ligação TCP
        fd = tcp_connect(cfg);

        // Construir mensagem CRE:
        // CRE UID password name event_date attendance_size Fname Fsize Fdata
        std::ostringstream oss;
        oss << "CRE "
            << state->uid << " "
            << state->pass << " "
            << name << " "
            << date << " "
            << time << " "
            << attendees << " "
            << fname << " "
            << file_data.size()
            << "\n";

        std::string header = oss.str();

        if (tcp_send_all(fd, header.data(), header.size()) < 0) {
            std::cerr << "Error sending CRE header.\n";
            ::close(fd);
            return;
        }

        // Enviar Fdata
        if (!file_data.empty()) {
            if (tcp_send_all(fd, file_data.data(), file_data.size()) < 0) {
                std::cerr << "Error sending CRE file data.\n";
                ::close(fd);
                return;
            }
        }

        // Ler resposta: "RCE status [EID]\n"
        std::string response = tcp_recv_line(fd);
        ::close(fd);
        fd = -1;

        if (response.empty()) {
            std::cerr << "Empty response to CRE.\n";
            return;
        }

        std::istringstream iss(response);
        std::string tag, status, eid;
        iss >> tag >> status >> eid;

        if (tag != "RCE" || status.empty()) {
            std::cerr << "Protocol error on create: " << response << "\n";
            return;
        }

        if (status == "OK") {
            std::cout << "Event created successfully with EID " << eid << ".\n";
        } else if (status == "NLG") {
            std::cout << "User not logged in (server side).\n";
            state->logged_in = false;
        } else if (status == "WRP") {
            std::cout << "Wrong password.\n";
        } else if (status == "NOK") {
            std::cout << "Event could not be created (NOK).\n";
        } else if (status == "ERR") {
            std::cout << "Error processing create request (ERR).\n";
        } else {
            std::cout << "Unknown status in create reply: " << status << "\n";
        }

    } catch (const std::exception &e) {
        if (fd >= 0) ::close(fd);
        std::cerr << "CREATE TCP error: " << e.what() << "\n";
    }
}


static void handle_show(ClientState * /*state*/,
                        const ClientNetConfig *cfg,
                        const char *line)
{
    std::string cmd, eid;
    {
        std::istringstream iss(line);
        iss >> cmd >> eid;
    }

    if (cmd != "show" || eid.empty()) {
        std::cerr << "Usage: show <EID>\n";
        return;
    }

    int fd = -1;

    try {
        fd = tcp_connect(cfg);

        // Enviar "SED EID\n"
        std::string request = "SED " + eid + "\n";
        if (tcp_send_all(fd, request.data(), request.size()) < 0) {
            std::cerr << "Error sending SED request.\n";
            ::close(fd);
            return;
        }

        // Ler linha de cabeçalho: "RSE status ...\n"
        std::string header = tcp_recv_line(fd);
        if (header.empty()) {
            std::cerr << "Empty response to SED.\n";
            ::close(fd);
            return;
        }

        std::istringstream iss(header);
        std::string tag, status;
        iss >> tag >> status;

        if (tag != "RSE" || status.empty()) {
            std::cerr << "Protocol error on show: " << header << "\n";
            ::close(fd);
            return;
        }

        if (status != "OK") {
            // Enunciado só define NOK/ERR além de OK
            if (status == "NOK") {
                std::cout << "Event " << eid << " does not exist or has no description file.\n";
            } else if (status == "ERR") {
                std::cout << "Error processing show request (ERR).\n";
            } else {
                std::cout << "Show failed with status: " << status << "\n";
            }
            ::close(fd);
            return;
        }

        // status == OK → ler o resto dos campos do header
        std::string owner_uid, name, date, time, fname;
        int attendance = 0;
        int reserved   = 0;
        long long fsize_ll = 0;
        int event_state = 1;

        if (!(iss >> owner_uid >> name >> date >> time
                  >> attendance >> reserved >> fname >> fsize_ll)) {
            std::cerr << "Malformed RSE header: " << header << "\n";
            ::close(fd);
            return;
        }

        iss >> event_state; 

        if (fsize_ll < 0 || fsize_ll > 10'000'000) {
            std::cerr << "Invalid file size in RSE: " << fsize_ll << "\n";
            ::close(fd);
            return;
        }

        size_t fsize = static_cast<size_t>(fsize_ll);

        // Ler Fdata (fsize bytes)
        std::string file_data;
        file_data.resize(fsize);
        if (fsize > 0) {
            if (!tcp_recv_exact(fd, file_data.data(), fsize)) {
                std::cerr << "Error receiving file data in RSE.\n";
                ::close(fd);
                return;
            }
        }

        ::close(fd);
        fd = -1;

        // Guardar ficheiro localmente com nome Fname
        std::ofstream fout(fname, std::ios::binary);
        if (!fout.is_open()) {
            std::cerr << "Could not create local file \"" << fname << "\".\n";
            return;
        }

        if (fsize > 0) {
            fout.write(file_data.data(), static_cast<std::streamsize>(fsize));
            if (!fout.good()) {
                std::cerr << "Error writing to local file \"" << fname << "\".\n";
                return;
            }
        }

        fout.close();

        // Mostrar info do evento
        std::cout << "Event " << eid << " (" << name << ")\n";
        std::cout << "  Owner UID : " << owner_uid << "\n";
        std::cout << "  Date/Time : " << date << " " << time << "\n";
        std::cout << "  Seats     : " << reserved
                  << " reserved / " << attendance << " total.\n";
        std::cout << "  File saved: ./" << fname
                  << " (" << fsize << " bytes)\n";

        // Informação extra: sold-out / possivelmente fechado
        // event_state: 0=Past, 1=Open, 2=SoldOut, 3=ClosedByUser
        if (event_state == 3) {
            std::cout << "  Status    : CLOSED by owner (no longer accepting reservations).\n";
        } else if (reserved >= attendance) {
            std::cout << "  Status    : SOLD OUT.\n";
        } else {
            std::cout << "  Status    : accepting reservations (or not sold out yet).\n";
        }

    } catch (const std::exception &e) {
        if (fd >= 0) ::close(fd);
        std::cerr << "SHOW TCP error: " << e.what() << "\n";
    }
}


void tcp_dispatch_command(ClientState *state,
                          const ClientNetConfig *cfg,
                          const char *line)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "list") {
        handle_list(state, cfg, line);
    } else if (cmd == "close") {
        handle_close(state, cfg, line);
    } else if (cmd == "reserve") {
        handle_reserve(state, cfg, line);
    } else if (cmd == "changepw" || cmd == "changePass") {
        handle_changepass(state, cfg, line);
    } else if (cmd == "create") {
        handle_create(state, cfg, line);   // <<< novo
    } else if (cmd == "show") {
        handle_show(state, cfg, line);     // <<< novo
    } else {
        std::cerr << "Unknown TCP command: " << cmd << "\n";
    }
}


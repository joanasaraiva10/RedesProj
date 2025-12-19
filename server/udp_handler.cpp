#include "udp_handler.h"
#include "udp.h"
#include "users.h"
#include "events.h"
#include "reservations.h"
#include "utils.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "utils.h"



// procura em EVENTS/*/RESERVATIONS/ um ficheiro com o nome dado.
// devolve true e eid_out se encontrar.
static bool find_event_for_resfile(const std::string &res_filename,
                                   std::string &eid_out)
{
    DIR *dir = ::opendir("EVENTS");
    if (!dir) return false;

    struct dirent *ent;
    while ((ent = ::readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        // directorias de 3 dígitos
        if (std::strlen(ent->d_name) != 3) continue;

        std::string eid = ent->d_name;
        std::string path =
            "EVENTS/" + eid + "/RESERVATIONS/" + res_filename;

        if (file_exists(path)) {
            ::closedir(dir);
            eid_out = eid;
            return true;
        }
    }

    ::closedir(dir);
    return false;
}

// resumo de uma reserva (para LMR/RMR)
struct ReservationSummary {
    std::string eid;
    std::string datetime; // "dd-mm-yyyy hh:mm:ss"
    int         seats{};
    std::time_t ts{};     // para ordenar (timestamp)
};

//  LIN 
static void handle_LIN(std::istringstream &iss, std::string &reply)
{
    std::string uid, pass, extra;
    if (!(iss >> uid >> pass) || (iss >> extra) ||
        !proto_valid_uid(uid) || !proto_valid_password(pass)) {
        reply = "RLI ERR\n";
        return;
    }

    UserStatus st = es_user_login(uid, pass);
    std::string status = user_status_to_string(st);
    reply = "RLI " + status + "\n";
}

// LOU 
static void handle_LOU(std::istringstream &iss, std::string &reply)
{
    std::string uid, pass, extra;
    if (!(iss >> uid >> pass) || (iss >> extra) ||
        !proto_valid_uid(uid) || !proto_valid_password(pass)) {
        reply = "RLO ERR\n";
        return;
    }

    UserStatus st = es_user_logout(uid, pass);
    std::string status = user_status_to_string(st);
    reply = "RLO " + status + "\n";
}

//  UNR 
static void handle_UNR(std::istringstream &iss, std::string &reply)
{
    std::string uid, pass, extra;
    if (!(iss >> uid >> pass) || (iss >> extra) ||
        !proto_valid_uid(uid) || !proto_valid_password(pass)) {
        reply = "RUR ERR\n";
        return;
    }

    UserStatus st = es_user_unregister(uid, pass);
    std::string status = user_status_to_string(st);
    reply = "RUR " + status + "\n";
}

//  LME (myevents) 
static void handle_LME(std::istringstream &iss, std::string &reply)
{
    std::string uid, pass, extra;
    if (!(iss >> uid >> pass) || (iss >> extra) ||
        !proto_valid_uid(uid) || !proto_valid_password(pass)) {
        reply = "RME ERR\n";
        return;
    }

    // password / login checks
    if (!es_user_exists(uid)) {
        // utilizador não existe ⇒ não tem eventos
        reply = "RME NOK\n";
        return;
    }

    if (!es_user_check_password(uid, pass)) {
        reply = "RME WRP\n";
        return;
    }

    if (!es_user_is_logged_in(uid)) {
        reply = "RME NLG\n";
        return;
    }

    std::string created_dir = "USERS/" + uid + "/CREATED";
    DIR *dir = ::opendir(created_dir.c_str());
    if (!dir) {
        reply = "RME NOK\n";
        return;
    }

    std::vector<std::string> eids;
    struct dirent *ent;
    while ((ent = ::readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue;

        // esperamos ficheiros "001.txt"
        std::string name = ent->d_name;
        if (name.size() != 7 || name.substr(3) != ".txt") continue;

        std::string eid = name.substr(0, 3);
        eids.push_back(eid);
    }
    ::closedir(dir);

    if (eids.empty()) {
        reply = "RME NOK\n";
        return;
    }

    std::sort(eids.begin(), eids.end());

    std::ostringstream out;
    out << "RME OK";

    for (const auto &eid : eids) {
        EventInfo info;
        if (!load_event(eid, info)) {
            continue; // ignora entradas estranhas
        }
        int st = static_cast<int>(info.state);
        out << " " << eid << " " << st;
    }
    out << "\n";
    reply = out.str();
}

//  LMR (myreservations) 
static void handle_LMR(std::istringstream &iss, std::string &reply)
{
    std::string uid, pass, extra;
    if (!(iss >> uid >> pass) || (iss >> extra) ||
        !proto_valid_uid(uid) || !proto_valid_password(pass)) {
        reply = "RMR ERR\n";
        return;
    }

    if (!es_user_exists(uid)) {
        // utilizador não existe → não tem reservas
        reply = "RMR NOK\n";
        return;
    }

    if (!es_user_check_password(uid, pass)) {
        reply = "RMR WRP\n";
        return;
    }

    if (!es_user_is_logged_in(uid)) {
        reply = "RMR NLG\n";
        return;
    }

    std::string reserved_dir = "USERS/" + uid + "/RESERVED";
    DIR *dir = ::opendir(reserved_dir.c_str());
    if (!dir) {
        // não há diretoria RESERVED → sem reservas
        reply = "RMR NOK\n";
        return;
    }

    std::vector<ReservationSummary> all;
    struct dirent *ent;

    while ((ent = ::readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue;

        std::string fname = ent->d_name; // ex: R-111111-2025-12-05 153000.txt

        // descobrir EID correspondente, olhando para EVENTS/*/RESERVATIONS/fname
        std::string eid;
        if (!find_event_for_resfile(fname, eid)) {
            continue; // ficheiro estranho/inconsistente
        }

        std::string path = reserved_dir + "/" + fname;
        std::ifstream in(path);
        if (!in.is_open()) continue;

        std::string line;
        if (!std::getline(in, line)) continue;

        std::istringstream ls(line);
        std::string file_uid;
        int seats = 0;
        std::string dt1, dt2;

        // formato: UID res_num res_datetime
        // res_datetime = "DD-MM-YYYY HH:MM:SS" -> dt1 + dt2
        if (!(ls >> file_uid >> seats >> dt1 >> dt2)) {
            continue;
        }

        std::string datetime = dt1 + " " + dt2;

        if (file_uid != uid || seats < 1 || seats > MAX_RESERVE_PEOPLE) {
            continue;
        }

        std::time_t ts = proto_parse_datetime_with_seconds(datetime);
        if (ts == 0) continue;

        ReservationSummary r;
        r.eid      = eid;
        r.datetime = datetime;
        r.seats    = seats;
        r.ts       = ts;
        all.push_back(std::move(r));
    }

    ::closedir(dir);

    if (all.empty()) {
        reply = "RMR NOK\n";
        return;
    }

    // ordenar por data/hora de reserva, mais recente primeiro
    std::sort(all.begin(), all.end(),
              [](const ReservationSummary &a,
                 const ReservationSummary &b) {
                  return a.ts > b.ts;
              });

    // Máximo 50 reservas (as 50 mais recentes)
    if (all.size() > 50) {
        all.resize(50);
    }

    std::ostringstream out;
    out << "RMR OK";
    for (const auto &r : all) {
        // protocolo: EID date value
        // date = "dd-mm-yyyy hh:mm:ss"
        out << " " << r.eid
            << " " << r.datetime
            << " " << r.seats;
    }
    out << "\n";

    reply = out.str();
}


void udp_handle_datagram(int udp_fd, bool verbose)
{
    char buf[2048];
    UdpPeer peer{};

    ssize_t n = udp_recv_datagram(udp_fd, buf, sizeof(buf) - 1, peer);
    if (n <= 0) {
        return; // erro ou datagrama vazio
    }
    buf[n] = '\0';

    if (verbose) {
    std::istringstream tmp(buf);  
    std::string cmd, uid;
    tmp >> cmd >> uid;

    if (!proto_valid_uid(uid)) uid = "------";
    if (cmd.empty()) cmd = "???";

    std::cout << "[ES][UDP] " << cmd
              << " UID=" << uid
              << " from " << inet_ntoa(peer.addr.sin_addr)
              << ":" << ntohs(peer.addr.sin_port)
              << "\n";
}


    std::string line(buf, n);
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    std::string reply;

    if (cmd == "LIN") {
        handle_LIN(iss, reply);
    } else if (cmd == "LOU") {
        handle_LOU(iss, reply);
    } else if (cmd == "UNR") {
        handle_UNR(iss, reply);
    } else if (cmd == "LME") {
        handle_LME(iss, reply);
    } else if (cmd == "LMR") {
        handle_LMR(iss, reply);
    } else {
        reply = "ERR\n";
    }

    if (!reply.empty()) {
        udp_send_datagram(udp_fd, reply.c_str(), reply.size(), peer);
    }
}

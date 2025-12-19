#include "tcp_handler.h"
#include "utils.h"
#include "users.h"
#include "events.h"
#include "reservations.h"
#include "protocol.h"

#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <fstream>
#include <ctime>
#include <string>


// helpers read/write exact
static bool read_exact_fd(int fd, void *buf, std::size_t n) {
    std::size_t done = 0;
    char *p = static_cast<char*>(buf);
    while (done < n) {
        ssize_t r = ::read(fd, p + done, n - done);
        if (r <= 0) return false;
        done += static_cast<std::size_t>(r);
    }
    return true;
}

static bool write_exact_fd(int fd, const void *buf, std::size_t n) {
    std::size_t done = 0;
    const char *p = static_cast<const char*>(buf);
    while (done < n) {
        ssize_t r = ::write(fd, p + done, n - done);
        if (r <= 0) return false;
        done += static_cast<std::size_t>(r);
    }
    return true;
}


// Reader com 1-byte pushback 
struct Reader {
    int fd;
    bool has_pb = false;
    char pb = 0;

    explicit Reader(int f) : fd(f) {}

    bool getch(char &c) {
        if (has_pb) { c = pb; has_pb = false; return true; }
        ssize_t r = ::read(fd, &c, 1);
        return r == 1;
    }

    void ungetch(char c) { has_pb = true; pb = c; }

    // lê token até ' ' ou '\n'
    bool read_token(std::string &tok) {
        tok.clear();
        char c;

        // skip leading spaces
        while (true) {
            if (!getch(c)) return false;
            if (c == ' ') continue;
            break;
        }

        while (true) {
            if (c == ' ' || c == '\n') {
                ungetch(c);
                break;
            }
            tok.push_back(c);
            if (!getch(c)) return false;
        }
        return !tok.empty();
    }

    bool expect_char(char expected) {
        char c;
        if (!getch(c)) return false;
        return c == expected;
    }

    bool expect_space()   { return expect_char(' '); }
    bool expect_newline() { return expect_char('\n'); }
};


static void tcp_verbose(bool verbose,
                        const char *ip,
                        uint16_t port,
                        const char *cmd,
                        const std::string &uid)
{
    if (!verbose) return;
    std::cout << "[ES][TCP] " << cmd
              << " UID=" << uid
              << " from " << ip << ":" << port
              << "\n";
}


// Handlers
static void handle_LST(int fd, Reader &rd, bool verbose, const char *ip, uint16_t port) {

    tcp_verbose(verbose, ip, port, "LST", "------");

    // LST\n
    if (!rd.expect_newline()) {
        const std::string resp = "RLS ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    auto events = load_all_events();
    if (events.empty()) {
        const std::string resp = "RLS NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    std::ostringstream out;
    out << "RLS OK";
    for (const auto &ev : events) {
        out << " " << ev.eid
            << " " << ev.name
            << " " << static_cast<int>(ev.state)
            << " " << ev.event_date; // "dd-mm-yyyy hh:mm" -> vira 2 tokens
    }
    out << "\n";

    const std::string resp = out.str();
    write_exact_fd(fd, resp.data(), resp.size());
}

static void handle_CRE(int fd, Reader &rd, bool verbose, const char *ip, uint16_t port) {
    // CRE UID PASS NAME dd-mm-yyyy hh:mm ATT Fname Fsize Fdata\n

    std::string uid, pass, name, date_part, time_part, att_s, fname, fsize_s;

    if (!rd.expect_space() || !rd.read_token(uid) ||
        !rd.expect_space() || !rd.read_token(pass) ||
        !rd.expect_space() || !rd.read_token(name) ||
        !rd.expect_space() || !rd.read_token(date_part) ||
        !rd.expect_space() || !rd.read_token(time_part) ||
        !rd.expect_space() || !rd.read_token(att_s) ||
        !rd.expect_space() || !rd.read_token(fname) ||
        !rd.expect_space() || !rd.read_token(fsize_s)) {
        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    // verbose por request (sem password)
    tcp_verbose(verbose, ip, port, "CRE", proto_valid_uid(uid) ? uid : "------");

    int attendance = 0;
    long long fsize_ll = -1;
    try {
        attendance = std::stoi(att_s);
        fsize_ll = std::stoll(fsize_s);
    } catch (...) {
        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    if (fsize_ll < 0 || fsize_ll > MAX_FILE_SIZE_BYTES) {
        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }
    const int fsize = static_cast<int>(fsize_ll);

    // validação do protocolo
    if (!proto_valid_uid(uid) ||
        !proto_valid_password(pass) ||
        !proto_valid_event_name(name) ||
        !proto_valid_date_ddmmyyyy(date_part) ||
        !proto_valid_time_hhmm(time_part) ||
        attendance < MIN_ATTENDANCE || attendance > MAX_ATTENDANCE ||
        !proto_valid_fname(fname)) {
        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    //tem de haver UM espaço entre Fsize e os bytes
    char sep = 0;
    if (!rd.getch(sep) || sep != ' ') {
        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    // ler Fdata
    std::string file_data;
    file_data.resize(static_cast<std::size_t>(fsize));
    if (fsize > 0) {
        if (!read_exact_fd(fd, file_data.data(), static_cast<std::size_t>(fsize))) {
            const std::string resp = "RCE NOK\n";
            write_exact_fd(fd, resp.data(), resp.size());
            return;
        }
    }

    // terminador final: \n (aceita \r\n também)
    char endc = 0;
    if (!rd.getch(endc)) {
        // EOF logo após bytes -> aceitável
    } else if (endc == '\n') {
        // ok
    } else if (endc == '\r') {
        char lf = 0;
        if (!rd.getch(lf) || lf != '\n') {
            const std::string resp = "RCE ERR\n";
            write_exact_fd(fd, resp.data(), resp.size());
            return;
        }
    } else {
        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    // auth
    if (!es_user_exists(uid) || !es_user_is_logged_in(uid)) {
        const std::string resp = "RCE NLG\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }
    if (!es_user_check_password(uid, pass)) {
        const std::string resp = "RCE WRP\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    // criar evento
    std::string eid;
    bool ok = es_create_event(uid, name, date_part, time_part, attendance, fname, file_data, eid);
    if (!ok) {
        const std::string resp = "RCE NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    const std::string resp = "RCE OK " + eid + "\n";
    write_exact_fd(fd, resp.data(), resp.size());
}

static void handle_RID(int fd, Reader &rd, bool verbose, const char *ip, uint16_t port) {
    // RID UID PASS EID people\n
    std::string uid, pass, eid, ppl_s;

    if (!rd.expect_space() || !rd.read_token(uid) ||
        !rd.expect_space() || !rd.read_token(pass) ||
        !rd.expect_space() || !rd.read_token(eid) ||
        !rd.expect_space() || !rd.read_token(ppl_s) ||
        !rd.expect_newline()) {
        const std::string resp = "RRI ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    tcp_verbose(verbose, ip, port, "RID", proto_valid_uid(uid) ? uid : "------");


    int people = 0;
    try { people = std::stoi(ppl_s); } catch (...) { people = 0; }

    if (!proto_valid_uid(uid) || !proto_valid_password(pass) || !proto_valid_eid(eid) ||
        people <= 0 || people > MAX_RESERVE_PEOPLE) {
        const std::string resp = "RRI ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    int remaining = 0;
    ReserveStatus st = es_make_reservation(uid, pass, eid, people, remaining);

    std::string resp;
    switch (st) {
        case ReserveStatus::ACC: resp = "RRI ACC\n"; break;
        case ReserveStatus::REJ: resp = "RRI REJ " + std::to_string(remaining) + "\n"; break;
        case ReserveStatus::CLS: resp = "RRI CLS\n"; break;
        case ReserveStatus::SLD: resp = "RRI SLD\n"; break;
        case ReserveStatus::PST: resp = "RRI PST\n"; break;
        case ReserveStatus::NLG: resp = "RRI NLG\n"; break;
        case ReserveStatus::WRP: resp = "RRI WRP\n"; break;
        default: resp = "RRI NOK\n"; break;
    }
    write_exact_fd(fd, resp.data(), resp.size());
}

static void handle_CLS(int fd, Reader &rd, bool verbose, const char *ip, uint16_t port) {
    // CLS UID PASS EID\n
    std::string uid, pass, eid;

    if (!rd.expect_space() || !rd.read_token(uid) ||
        !rd.expect_space() || !rd.read_token(pass) ||
        !rd.expect_space() || !rd.read_token(eid) ||
        !rd.expect_newline()) {
        const std::string resp = "RCL ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    tcp_verbose(verbose, ip, port, "CLS", proto_valid_uid(uid) ? uid : "------");


    if (!proto_valid_uid(uid) || !proto_valid_password(pass) || !proto_valid_eid(eid)) {
        const std::string resp = "RCL ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    if (!es_user_exists(uid) || !es_user_check_password(uid, pass)) {
        const std::string resp = "RCL NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    if (!es_user_is_logged_in(uid)) {
        const std::string resp = "RCL NLG\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    EventInfo ev;
    if (!load_event(eid, ev)) {
        const std::string resp = "RCL NOE\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    if (ev.owner_uid != uid) {
        const std::string resp = "RCL EOW\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    switch (ev.state) {
        case EventState::SoldOut: {
            const std::string resp = "RCL SLD\n";
            write_exact_fd(fd, resp.data(), resp.size());
            return;
        }
        case EventState::Past: {
            (void)ensure_end_if_past(eid, ev.event_date);
            const std::string resp = "RCL PST\n";
            write_exact_fd(fd, resp.data(), resp.size());
            return;
        }
        case EventState::ClosedByUser: {
            const std::string resp = "RCL CLO\n";
            write_exact_fd(fd, resp.data(), resp.size());
            return;
        }
        case EventState::Open:
        default:
            break;
    }

    // criar END
    const std::string end_path = event_dir(eid) + "/END " + eid + ".txt";

    std::time_t now = std::time(nullptr);
    std::tm *lt = std::localtime(&now);
    if (!lt) {
        const std::string resp = "RCL NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", lt) == 0) {
        const std::string resp = "RCL NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    std::ofstream out(end_path);
    if (!out.is_open()) {
        const std::string resp = "RCL NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }
    out << buf << "\n";
    if (!out.good()) {
        const std::string resp = "RCL NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    const std::string resp = "RCL OK\n";
    write_exact_fd(fd, resp.data(), resp.size());
}

static void handle_SED(int fd, Reader &rd, bool verbose, const char *ip, uint16_t port) {

     tcp_verbose(verbose, ip, port, "SED", "------");

    // SED EID\n -> RSE OK ... Fsize Fdata\n
    std::string eid;

    if (!rd.expect_space() || !rd.read_token(eid) || !rd.expect_newline()) {
        const std::string resp = "RSE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    if (!proto_valid_eid(eid)) {
        const std::string resp = "RSE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    EventInfo ev;
    if (!load_event(eid, ev)) {
        const std::string resp = "RSE NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    if (ev.state == EventState::Past) {
        (void)ensure_end_if_past(eid, ev.event_date);
    }

    const std::string desc_path = event_dir(eid) + "/DESCRIPTION/" + ev.desc_fname;
    std::ifstream f(desc_path, std::ios::binary);
    if (!f.is_open()) {
        const std::string resp = "RSE NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    std::string fdata((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    const int fsize = static_cast<int>(fdata.size());

    // header termina com SPACE e depois vem Fdata e no fim '\n'
    std::ostringstream hdr;
    hdr << "RSE OK "
        << ev.owner_uid << " "
        << ev.name << " "
        << ev.event_date << " "
        << ev.capacity << " "
        << ev.reserved << " "
        << ev.desc_fname << " "
        << fsize << " ";

    const std::string header = hdr.str();
    if (!write_exact_fd(fd, header.data(), header.size())) return;

    if (fsize > 0) {
        if (!write_exact_fd(fd, fdata.data(), static_cast<std::size_t>(fsize))) return;
    }

    const char nl = '\n';
    write_exact_fd(fd, &nl, 1);
}

static void handle_CPS(int fd, Reader &rd, bool verbose, const char *ip, uint16_t port) {
    // CPS UID old new\n
    std::string uid, oldp, newp;

    if (!rd.expect_space() || !rd.read_token(uid) ||
        !rd.expect_space() || !rd.read_token(oldp) ||
        !rd.expect_space() || !rd.read_token(newp) ||
        !rd.expect_newline()) {
        const std::string resp = "RCP ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    tcp_verbose(verbose, ip, port, "CPS", proto_valid_uid(uid) ? uid : "------");

    if (!proto_valid_uid(uid) || !proto_valid_password(oldp) || !proto_valid_password(newp)) {
        const std::string resp = "RCP ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    UserStatus st = es_user_change_password(uid, oldp, newp);
    const std::string resp = "RCP " + user_status_to_string(st) + "\n";
    write_exact_fd(fd, resp.data(), resp.size());
}


void tcp_handle_connection(int fd, bool verbose, const char *ip, uint16_t port)
{
    Reader rd(fd);

    std::string tag;
    if (!rd.read_token(tag)) { ::close(fd); return; }

    if (tag == "LST") handle_LST(fd, rd, verbose, ip, port);
    else if (tag == "CRE") handle_CRE(fd, rd, verbose, ip, port);
    else if (tag == "RID") handle_RID(fd, rd, verbose, ip, port);
    else if (tag == "CLS") handle_CLS(fd, rd, verbose, ip, port);
    else if (tag == "SED") handle_SED(fd, rd, verbose, ip, port);
    else if (tag == "CPS") handle_CPS(fd, rd, verbose, ip, port);
    else {
        if (verbose) tcp_verbose(verbose, ip, port, tag.c_str(), "------");
        const std::string resp = "ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
    }

    ::close(fd);
}


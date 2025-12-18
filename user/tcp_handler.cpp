#include "tcp_handler.h"
#include "utils.h"
#include "users.h"
#include "events.h"
#include "reservations.h"
#include "protocol.h"

#include <unistd.h>
#include <cstring>
#include <sstream>
#include <fstream>
#include <ctime>
#include <string>
#include <limits>

// ============================================================
// Low-level helpers (token-based parsing from TCP stream)
// ============================================================

static bool read_exact_fd(int fd, void *buf, std::size_t n)
{
    std::size_t done = 0;
    char *p = static_cast<char *>(buf);

    while (done < n) {
        ssize_t r = ::read(fd, p + done, n - done);
        if (r <= 0) return false;
        done += static_cast<std::size_t>(r);
    }
    return true;
}

static bool write_exact_fd(int fd, const void *buf, std::size_t n)
{
    std::size_t done = 0;
    const char *p = static_cast<const char *>(buf);

    while (done < n) {
        ssize_t r = ::write(fd, p + done, n - done);
        if (r <= 0) return false;
        done += static_cast<std::size_t>(r);
    }
    return true;
}

// skip spaces (not newlines)  [NOT USED - sockets can't lseek safely]
static bool skip_spaces(int fd)
{
    while (true) {
        char c;
        ssize_t r = ::read(fd, &c, 1);
        if (r <= 0) return false;
        if (c == ' ') continue;

        // sockets can't lseek; this helper is unsafe in sockets
        if (::lseek(fd, -1, SEEK_CUR) == (off_t)-1) {
            return false;
        }
        return true;
    }
}

// We'll implement our own pushback (because sockets can't lseek)
struct Reader {
    int fd;
    bool has_pb = false;
    char pb = 0;

    explicit Reader(int f) : fd(f) {}

    bool getch(char &c)
    {
        if (has_pb) {
            c = pb;
            has_pb = false;
            return true;
        }
        ssize_t r = ::read(fd, &c, 1);
        return r == 1;
    }

    void ungetch(char c)
    {
        has_pb = true;
        pb = c;
    }

    bool read_token(std::string &tok)
    {
        tok.clear();
        char c;

        // skip leading spaces
        while (true) {
            if (!getch(c)) return false;
            if (c == ' ') continue;
            break;
        }

        // token until space or '\n'
        while (true) {
            if (c == ' ' || c == '\n') {
                // delimiter belongs to stream for later logic
                ungetch(c);
                break;
            }
            tok.push_back(c);
            if (!getch(c)) return false;
        }
        return !tok.empty();
    }

    bool expect_char(char expected)
    {
        char c;
        if (!getch(c)) return false;
        return c == expected;
    }

    // consume exactly one space (protocol uses single spaces between items)
    bool expect_space()   { return expect_char(' '); }
    bool expect_newline() { return expect_char('\n'); }
};

// read int from token safely
static bool token_to_int(const std::string &s, int &out)
{
    try {
        size_t idx = 0;
        long long v = std::stoll(s, &idx, 10);
        if (idx != s.size()) return false;
        if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================
// Handlers
// ============================================================

// LST\n
static void handle_LST(int fd, Reader &rd)
{
    // after "LST" must be '\n'
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

    // RLS OK [EID name state dd-mm-yyyy hh:mm]... \n
    std::ostringstream out;
    out << "RLS OK";

    for (const auto &ev : events) {
        // ev.event_date is "dd-mm-yyyy hh:mm"
        out << " " << ev.eid
            << " " << ev.name
            << " " << static_cast<int>(ev.state)
            << " " << ev.event_date; // contains a space -> becomes two tokens, as required
    }

    out << "\n";
    std::string resp = out.str();
    write_exact_fd(fd, resp.data(), resp.size());
}

// CRE UID password name dd-mm-yyyy hh:mm attendance Fname Fsize␠Fdata\n
static void handle_CRE(int fd, Reader &rd)
{
    std::string uid, pass, name, date_part, time_part, attendance_s, fname, fsize_s;

    if (!rd.expect_space() || !rd.read_token(uid) ||
        !rd.expect_space() || !rd.read_token(pass) ||
        !rd.expect_space() || !rd.read_token(name) ||
        !rd.expect_space() || !rd.read_token(date_part) ||
        !rd.expect_space() || !rd.read_token(time_part) ||
        !rd.expect_space() || !rd.read_token(attendance_s) ||
        !rd.expect_space() || !rd.read_token(fname) ||
        !rd.expect_space() || !rd.read_token(fsize_s)) {

        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    int attendance = 0, fsize = 0;
    if (!token_to_int(attendance_s, attendance) || !token_to_int(fsize_s, fsize)) {
        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    // validate by protocol constraints
    if (!proto_valid_uid(uid) ||
        !proto_valid_password(pass) ||
        !proto_valid_event_name(name) ||
        !proto_valid_date_ddmmyyyy(date_part) ||
        !proto_valid_time_hhmm(time_part) ||
        attendance < 10 || attendance > 999 ||
        !proto_valid_fname(fname) ||
        fsize < 0 || fsize > MAX_FILE_SIZE_BYTES) {

        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    // After Fsize there MUST be one space, then Fdata, then '\n' at end of message.
    if (!rd.expect_space()) {
        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    // read Fdata
    std::string file_data;
    file_data.resize(static_cast<std::size_t>(fsize));

    if (fsize > 0) {
        if (!read_exact_fd(fd, file_data.data(), static_cast<std::size_t>(fsize))) {
            const std::string resp = "RCE NOK\n";
            write_exact_fd(fd, resp.data(), resp.size());
            return;
        }
    }

    // message must end with '\n'
    if (!rd.expect_newline()) {
        const std::string resp = "RCE ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    // auth checks
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

// RID UID password EID people\n
static void handle_RID(int fd, Reader &rd)
{
    std::string uid, pass, eid, people_s;

    if (!rd.expect_space() || !rd.read_token(uid) ||
        !rd.expect_space() || !rd.read_token(pass) ||
        !rd.expect_space() || !rd.read_token(eid) ||
        !rd.expect_space() || !rd.read_token(people_s) ||
        !rd.expect_newline()) {

        const std::string resp = "RRI ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    int people = 0;
    if (!token_to_int(people_s, people)) {
        const std::string resp = "RRI ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    if (!proto_valid_uid(uid) || !proto_valid_password(pass) || !proto_valid_eid(eid) ||
        people < 1 || people > 999) {
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

// CLS UID password EID\n
static void handle_CLS(int fd, Reader &rd)
{
    std::string uid, pass, eid;

    if (!rd.expect_space() || !rd.read_token(uid) ||
        !rd.expect_space() || !rd.read_token(pass) ||
        !rd.expect_space() || !rd.read_token(eid) ||
        !rd.expect_newline()) {

        const std::string resp = "RCL ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

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

    // create END <eid>.txt with current timestamp
    std::string end_path = event_dir(eid) + "/END " + eid + ".txt";

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

// SED EID\n -> RSE OK ... Fsize␠Fdata\n
static void handle_SED(int fd, Reader &rd)
{
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

    std::string desc_path = event_dir(eid) + "/DESCRIPTION/" + ev.desc_fname;
    std::ifstream f(desc_path, std::ios::binary);
    if (!f.is_open()) {
        const std::string resp = "RSE NOK\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    std::string fdata((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    int fsize = static_cast<int>(fdata.size());

    // RSE OK UID name dd-mm-yyyy hh:mm attendance_size Seats_reserved Fname Fsize␠Fdata\n
    std::ostringstream hdr;
    hdr << "RSE OK "
        << ev.owner_uid << " "
        << ev.name << " "
        << ev.event_date << " "
        << ev.capacity << " "
        << ev.reserved << " "
        << ev.desc_fname << " "
        << fsize << " ";

    std::string header = hdr.str();
    if (!write_exact_fd(fd, header.data(), header.size())) return;

    if (fsize > 0) {
        if (!write_exact_fd(fd, fdata.data(), static_cast<std::size_t>(fsize))) return;
    }

    const char nl = '\n';
    write_exact_fd(fd, &nl, 1);
}

// CPS UID old new\n
static void handle_CPS(int fd, Reader &rd)
{
    std::string uid, oldp, newp;

    if (!rd.expect_space() || !rd.read_token(uid) ||
        !rd.expect_space() || !rd.read_token(oldp) ||
        !rd.expect_space() || !rd.read_token(newp) ||
        !rd.expect_newline()) {

        const std::string resp = "RCP ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    if (!proto_valid_uid(uid) || !proto_valid_password(oldp) || !proto_valid_password(newp)) {
        const std::string resp = "RCP ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        return;
    }

    UserStatus st = es_user_change_password(uid, oldp, newp);
    std::string resp = "RCP " + user_status_to_string(st) + "\n";
    write_exact_fd(fd, resp.data(), resp.size());
}

// ============================================================
// Entry point: handle ONE TCP connection (ONE command)
// ============================================================

void tcp_handle_connection(int fd)
{
    Reader rd(fd);

    std::string tag;
    if (!rd.read_token(tag)) {
        const std::string resp = "ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
        ::close(fd);
        return;
    }

    if      (tag == "LST") handle_LST(fd, rd);
    else if (tag == "CRE") handle_CRE(fd, rd);
    else if (tag == "RID") handle_RID(fd, rd);
    else if (tag == "CLS") handle_CLS(fd, rd);
    else if (tag == "SED") handle_SED(fd, rd);
    else if (tag == "CPS") handle_CPS(fd, rd);
    else {
        const std::string resp = "ERR\n";
        write_exact_fd(fd, resp.data(), resp.size());
    }

    ::close(fd);
}

#include "events.h"

#include "utils.h"      // file_exists, read_first_line
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>       // sscanf, snprintf
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <filesystem>

// ---- lock inter-processo (fork-safe) ----
#include <sys/file.h>
#include <fcntl.h>

namespace fs = std::filesystem;

// ============================================================
// Helpers: paths
// ============================================================

// "EVENTS/<eid>"
std::string event_dir(const std::string &eid) {
    return std::string("EVENTS/") + eid;
}

// lockfile global para BD (EVENTS/.lock)
static const char* EVENTS_LOCK_PATH = "EVENTS/.lock";

// ============================================================
// Helpers: inter-process lock (flock)
// ============================================================

struct EventsFsLock {
    int fd = -1;

    explicit EventsFsLock(const char* path = EVENTS_LOCK_PATH) {
        // garantir que o diretório EVENTS existe antes de abrir lock
        std::error_code ec;
        if (!fs::exists("EVENTS", ec)) {
            fs::create_directory("EVENTS", ec);
        }

        fd = ::open(path, O_CREAT | O_RDWR, 0666);
        if (fd < 0) return;

        if (::flock(fd, LOCK_EX) < 0) {
            ::close(fd);
            fd = -1;
            return;
        }
    }

    ~EventsFsLock() {
        if (fd >= 0) {
            ::flock(fd, LOCK_UN);
            ::close(fd);
        }
    }

    bool ok() const { return fd >= 0; }
};

// ============================================================
// Date parsing
// ============================================================

// Parse "dd-mm-yyyy hh:mm" -> struct tm
bool parse_event_datetime(const std::string &event_date, struct tm &out_tm) {
    int day = 0, month = 0, year = 0, hour = 0, minute = 0;
    if (std::sscanf(event_date.c_str(), "%2d-%2d-%4d %2d:%2d",
                    &day, &month, &year, &hour, &minute) != 5) {
        return false;
    }

    std::memset(&out_tm, 0, sizeof(out_tm));
    out_tm.tm_mday = day;
    out_tm.tm_mon  = month - 1;
    out_tm.tm_year = year - 1900;
    out_tm.tm_hour = hour;
    out_tm.tm_min  = minute;
    out_tm.tm_sec  = 0;

    return true;
}

// Parse "dd-mm-yyyy hh:mm:ss" -> struct tm
static bool parse_datetime_with_seconds(const std::string &s, struct tm &out_tm) {
    int day = 0, month = 0, year = 0, hour = 0, minute = 0, sec = 0;
    if (std::sscanf(s.c_str(), "%2d-%2d-%4d %2d:%2d:%2d",
                    &day, &month, &year, &hour, &minute, &sec) != 6) {
        return false;
    }

    std::memset(&out_tm, 0, sizeof(out_tm));
    out_tm.tm_mday = day;
    out_tm.tm_mon  = month - 1;
    out_tm.tm_year = year - 1900;
    out_tm.tm_hour = hour;
    out_tm.tm_min  = minute;
    out_tm.tm_sec  = sec;

    return true;
}

// ============================================================
// Reserved seats
// ============================================================

// Lê o total de reservas de "RES <eid>.txt"
static int read_total_reserved(const std::string &eid) {
    std::string path = event_dir(eid) + "/RES " + eid + ".txt";

    std::ifstream in(path);
    if (!in.is_open()) {
        return 0; // se não existir, consideramos 0
    }

    int value = 0;
    in >> value;
    if (!in.good()) {
        return 0;
    }
    return value;
}

// ============================================================
// Compute state
// ============================================================

static EventState compute_state(const std::string &eid,
                                const std::string &event_date_str,
                                int capacity,
                                int reserved,
                                bool has_end_file,
                                bool &closed_by_user_out)
{
    closed_by_user_out = false;

    struct tm event_tm{};
    if (!parse_event_datetime(event_date_str, event_tm)) {
        return EventState::Past;
    }

    const time_t event_ts = std::mktime(&event_tm);

    // Se existe END, distingue "Past" vs "ClosedByUser"
    if (has_end_file) {
        const std::string end_path = event_dir(eid) + "/END " + eid + ".txt";
        std::string line;

        if (read_first_line(end_path, line)) {
            struct tm end_tm{};
            if (parse_datetime_with_seconds(line, end_tm)) {
                const time_t end_ts = std::mktime(&end_tm);

                // Heurística: se END != data do evento => fechado pelo dono;
                // se END == data do evento => terminou automaticamente por "Past".
                if (end_ts != event_ts) {
                    closed_by_user_out = true;
                    return EventState::ClosedByUser;
                }
                return EventState::Past;
            }

            // END mal formado -> consideramos fechado pelo dono
            closed_by_user_out = true;
            return EventState::ClosedByUser;
        }

        // Não conseguiu ler END -> considera fechado pelo dono
        closed_by_user_out = true;
        return EventState::ClosedByUser;
    }

    // Sem END: decide por tempo e por lotação
    const time_t now_ts = std::time(nullptr);

    if (now_ts > event_ts) {
        return EventState::Past;
    }

    if (capacity > 0 && reserved >= capacity) {
        return EventState::SoldOut;
    }

    return EventState::Open;
}

// ============================================================
// Ensure END if Past (now fork-safe)
// ============================================================

bool ensure_end_if_past(const std::string &eid, const std::string &event_date_str)
{
    // LOCK global: evita 2 processos escreverem END ao mesmo tempo
    EventsFsLock lock;
    if (!lock.ok()) return false;

    const std::string end_path = event_dir(eid) + "/END " + eid + ".txt";

    if (file_exists(end_path)) {
        return true; // já existe
    }

    // event_date_str: "dd-mm-yyyy hh:mm"
    int day=0, month=0, year=0, hour=0, min=0;
    if (std::sscanf(event_date_str.c_str(), "%2d-%2d-%4d %2d:%2d",
                    &day, &month, &year, &hour, &min) != 5) {
        return false;
    }

    std::tm tm_end{};
    tm_end.tm_mday = day;
    tm_end.tm_mon  = month - 1;
    tm_end.tm_year = year - 1900;
    tm_end.tm_hour = hour;
    tm_end.tm_min  = min;
    tm_end.tm_sec  = 0;

    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", &tm_end) == 0) {
        return false;
    }

    std::ofstream out(end_path);
    if (!out.is_open()) return false;

    out << buf << "\n";
    return out.good();
}

// ============================================================
// Public API: load_event / load_all_events
// ============================================================

bool load_event(const std::string &eid, EventInfo &out) {
    const std::string base = event_dir(eid);
    const std::string start_path = base + "/START " + eid + ".txt";

    std::string line;
    if (!read_first_line(start_path, line) || line.empty()) {
        return false;
    }

    // START: UID name desc_fname event_attend start_date start_time
    std::istringstream iss(line);
    std::string uid, name, desc_fname;
    int capacity = 0;
    std::string date_part, time_part;

    if (!(iss >> uid >> name >> desc_fname >> capacity >> date_part >> time_part)) {
        return false;
    }

    EventInfo info;
    info.eid        = eid;
    info.owner_uid  = uid;
    info.name       = name;
    info.desc_fname = desc_fname;
    info.capacity   = capacity;
    info.event_date = date_part + " " + time_part;

    struct tm tmp{};
    if (!parse_event_datetime(info.event_date, tmp)) {
        return false;
    }

    info.reserved = read_total_reserved(eid);

    const std::string end_path = base + "/END " + eid + ".txt";
    info.has_end_file = file_exists(end_path);

    bool closed_by_user = false;
    info.state = compute_state(eid,
                               info.event_date,
                               info.capacity,
                               info.reserved,
                               info.has_end_file,
                               closed_by_user);
    info.closed_by_user = closed_by_user;

    out = std::move(info);
    return true;
}

std::vector<EventInfo> load_all_events() {
    std::vector<EventInfo> events;

    DIR *dir = ::opendir("EVENTS");
    if (!dir) {
        return events; // não há diretoria EVENTS
    }

    std::vector<std::string> eids;
    struct dirent *ent;

    while ((ent = ::readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        if (std::strlen(ent->d_name) != 3) continue;
        eids.emplace_back(ent->d_name);
    }

    ::closedir(dir);

    std::sort(eids.begin(), eids.end());

    for (const auto &eid : eids) {
        EventInfo info;
        if (load_event(eid, info)) {
            events.push_back(std::move(info));
        }
    }

    return events;
}

// ============================================================
// Event creation (CRE) - best concurrency-safe solution
// ============================================================

static void ensure_events_root()
{
    std::error_code ec;
    if (!fs::exists("EVENTS", ec)) {
        fs::create_directory("EVENTS", ec);
    }
}

// Cria de forma atómica o diretório do evento e devolve EID + base.
// Estratégia: tentar mkdir(EVENTS/001), mkdir(EVENTS/002), ...
// O mkdir é atómico -> não há colisões mesmo com processos simultâneos.
static bool allocate_and_create_event_dir(std::string &eid_out, std::string &base_out)
{
    ensure_events_root();

    for (int i = 1; i <= 999; ++i) {
        char buf[4];
        std::snprintf(buf, sizeof(buf), "%03d", i);

        std::string eid(buf);
        std::string base = event_dir(eid);

        std::error_code ec;
        bool created = fs::create_directory(base, ec);
        if (created && !ec) {
            eid_out = std::move(eid);
            base_out = std::move(base);
            return true;
        }

        // Se já existe, tenta o próximo.
        // Para qualquer outro erro, continuamos a tentar (pode ser condição transitória),
        // mas se quiseres ser mais "estrita", podes return false aqui.
    }
    return false;
}

bool es_create_event(const std::string &uid,
                     const std::string &name,
                     const std::string &date_part,
                     const std::string &time_part,
                     int attendance,
                     const std::string &fname,
                     const std::string &file_data,
                     std::string &eid_out)
{
    // LOCK global: com fork, isto é essencial para evitar colisões
    // em USERS/<uid>/CREATED e noutras escritas relacionadas.
    EventsFsLock lock;
    if (!lock.ok()) return false;

    std::error_code ec;

    std::string base;
    if (!allocate_and_create_event_dir(eid_out, base)) {
        return false;
    }
    const std::string eid = eid_out;

    // START
    {
        const std::string start_path = base + "/START " + eid + ".txt";
        std::ofstream out(start_path);
        if (!out.is_open()) return false;

        out << uid << " "
            << name << " "
            << fname << " "
            << attendance << " "
            << date_part << " "
            << time_part << "\n";

        if (!out.good()) return false;
    }

    // RES
    {
        const std::string res_path = base + "/RES " + eid + ".txt";
        std::ofstream out(res_path);
        if (!out.is_open()) return false;

        out << 0 << "\n";
        if (!out.good()) return false;
    }

    // DESCRIPTION/Fname
    {
        const std::string desc_dir = base + "/DESCRIPTION";
        fs::create_directory(desc_dir, ec);
        if (ec) return false;

        const std::string fpath = desc_dir + "/" + fname;
        std::ofstream out(fpath, std::ios::binary);
        if (!out.is_open()) return false;

        out.write(file_data.data(),
                  static_cast<std::streamsize>(file_data.size()));
        if (!out.good()) return false;
    }

    // RESERVATIONS/
    {
        const std::string resdir = base + "/RESERVATIONS";
        fs::create_directory(resdir, ec);
        if (ec) return false;
    }

    // USERS/UID/CREATED/EID.txt
    {
        const std::string created_dir = "USERS/" + uid + "/CREATED";
        fs::create_directories(created_dir, ec);
        if (ec) return false;

        const std::string cpath = created_dir + "/" + eid + ".txt";
        std::ofstream out(cpath);
        if (!out.is_open()) return false;
        if (!out.good()) return false;
    }

    return true;
}

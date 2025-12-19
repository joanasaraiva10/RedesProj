#include "protocol.h"
#include <cctype>
#include <cstdio>

// UID: 6 dígitos
bool proto_valid_uid(const std::string &uid)
{
    if (uid.size() != UID_LEN) return false;
    for (char c : uid) {
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;
    }
    return true;
}

// Password: 8 alfanuméricos
bool proto_valid_password(const std::string &pass)
{
    if (pass.size() != PASSWORD_LEN) return false;
    for (char c : pass) {
        if (!std::isalnum(static_cast<unsigned char>(c)))
            return false;
    }
    return true;
}

// Nome do evento: 1..10 alfanuméricos
bool proto_valid_event_name(const std::string &name)
{
    if (name.empty() || name.size() > EVENT_NAME_MAX) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)))
            return false;
    }
    return true;
}


bool proto_valid_fname(const std::string &fname)
{
    if (fname.empty() || fname.size() > FNAME_MAX) return false;

    for (char c : fname) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) ||
              c == '-' || c == '_' || c == '.')) {
            return false;
        }
    }

    // verificar extensão .xxx
    auto pos = fname.rfind('.');
    if (pos == std::string::npos) return false;
    if (pos + 4 != fname.size()) return false; // '.' + 3 letras

    for (std::size_t i = pos + 1; i < fname.size(); ++i) {
        if (!std::isalpha(static_cast<unsigned char>(fname[i])))
            return false;
    }

    return true;
}

// EID: 3 dígitos
bool proto_valid_eid(const std::string &eid)
{
    if (eid.size() != 3) return false;
    for (char c : eid) {
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;
    }
    return true;
}

// "dd-mm-yyyy"
bool proto_valid_date_ddmmyyyy(const std::string &date)
{
    if (date.size() != 10) return false;
    if (date[2] != '-' || date[5] != '-') return false;

    std::size_t idxs[] = {0u,1u,3u,4u,6u,7u,8u,9u};
    for (std::size_t i : idxs) {
        if (!std::isdigit(static_cast<unsigned char>(date[i])))
            return false;
    }
    return true;
}

// "hh:mm"
bool proto_valid_time_hhmm(const std::string &time)
{
    if (time.size() != 5) return false;
    if (time[2] != ':') return false;
    if (!std::isdigit(static_cast<unsigned char>(time[0])) ||
        !std::isdigit(static_cast<unsigned char>(time[1])) ||
        !std::isdigit(static_cast<unsigned char>(time[3])) ||
        !std::isdigit(static_cast<unsigned char>(time[4]))) {
        return false;
    }
    return true;
}

// "dd-mm-yyyy hh:mm:ss"
bool proto_valid_datetime_with_seconds(const std::string &dt)
{
    if (dt.size() != 19) return false;
    if (dt[2] != '-' || dt[5] != '-' || dt[10] != ' ' ||
        dt[13] != ':' || dt[16] != ':') return false;

    int idxs[] = {0,1,3,4,6,7,8,9, 11,12,14,15,17,18};
    for (int i : idxs) {
        char c = dt[static_cast<std::size_t>(i)];
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;
    }
    return true;
}

// Parse "dd-mm-yyyy hh:mm:ss" -> time_t
std::time_t proto_parse_datetime_with_seconds(const std::string &s)
{
    if (!proto_valid_datetime_with_seconds(s)) {
        return 0;
    }

    int d=0, m=0, y=0, H=0, M=0, S=0;
    if (std::sscanf(s.c_str(), "%2d-%2d-%4d %2d:%2d:%2d",
                    &d,&m,&y,&H,&M,&S) != 6) {
        return 0;
    }

    std::tm tm{};
    tm.tm_mday = d;
    tm.tm_mon  = m - 1;
    tm.tm_year = y - 1900;
    tm.tm_hour = H;
    tm.tm_min  = M;
    tm.tm_sec  = S;

    return std::mktime(&tm);
}

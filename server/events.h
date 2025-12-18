#pragma once

#include <string>
#include <vector>
#include <ctime>

// Estados de evento (iguais aos do enunciado RLS/RME):
// 0 – evento no passado
// 1 – evento no futuro e ainda a aceitar reservas
// 2 – evento no futuro, mas esgotado (sold out)
// 3 – evento fechado explicitamente pelo utilizador
enum class EventState : int {
    Past          = 0,
    Open          = 1,
    SoldOut       = 2,
    ClosedByUser  = 3
};

struct EventInfo {
    std::string eid;         // "001"
    std::string owner_uid;   // UID de quem criou
    std::string name;        // nome curto
    std::string desc_fname;  // nome do ficheiro de descrição
    std::string event_date;  // "dd-mm-yyyy hh:mm"
    int         capacity = 0;
    int         reserved = 0;
    EventState  state   = EventState::Past;

    bool        has_end_file   = false;
    bool        closed_by_user = false;
};

// Caminho "EVENTS/<eid>"
std::string event_dir(const std::string &eid);

// Lê 1 evento a partir de EVENTS/eid (START/RES/END, etc.)
bool load_event(const std::string &eid, EventInfo &out);

// Lê todos os eventos em EVENTS/, ordenados por EID
std::vector<EventInfo> load_all_events();

// Parse "dd-mm-yyyy hh:mm" -> struct tm
bool parse_event_datetime(const std::string &event_date, struct tm &out_tm);

// Criação de evento (usado pelo comando CRE)
bool es_create_event(const std::string &uid,
                     const std::string &name,
                     const std::string &date_part,
                     const std::string &time_part,
                     int attendance,
                     const std::string &fname,
                     const std::string &file_data,
                     std::string &eid_out);

bool ensure_end_if_past(const std::string &eid, const std::string &event_date_str);


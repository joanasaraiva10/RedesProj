#include "reservations.h"
#include "users.h"
#include "events.h"
#include "utils.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <ctime>
#include <cstdio>

namespace fs = std::filesystem;

// Caminho EVENTS/eid/RES eid.txt
static std::string res_file(const std::string &eid) {
    return event_dir(eid) + "/RES " + eid + ".txt";
}

// Escreve "value\n"
static bool write_int_file(const std::string &path, int value)
{
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << value << "\n";
    return out.good();
}

// Gera o nome do ficheiro de reserva e a string data/hora a escrever
//  - filename: R-UID-YYYY-MM-DD HHMMSS.txt
//  - datetime_str: DD-MM-YYYY HH:MM:SS
static void make_reservation_names(const std::string &uid,
                                   std::string &filename_out,
                                   std::string &datetime_str_out)
{
    std::time_t now = std::time(nullptr);
    std::tm *lt = std::localtime(&now);
    if (!lt) {
        filename_out.clear();
        datetime_str_out.clear();
        return;
    }

    char date_file[11];     // YYYY-MM-DD
    char time_file[7];      // HHMMSS
    char datetime[20];      // DD-MM-YYYY HH:MM:SS

    std::strftime(date_file, sizeof(date_file), "%Y-%m-%d", lt);
    std::strftime(time_file, sizeof(time_file), "%H%M%S", lt);
    std::strftime(datetime, sizeof(datetime), "%d-%m-%Y %H:%M:%S", lt);

    filename_out = "R-" + uid + "-" + std::string(date_file) +
                   " " + std::string(time_file) + ".txt";
    datetime_str_out = datetime;
}

// Se o evento já passou mas ainda não tem END, podes opcionalmente
// criar END EID.txt com a data/hora de ocorrência (igual à do evento).
// Isto segue a sugestão do guia 2.1, mas não é estritamente obrigatório.
static void maybe_create_end_for_past_event(const std::string &eid,
                                            const std::string &event_date_str)
{
    // Se já existir END, não fazemos nada.
    std::string end_path = event_dir(eid) + "/END " + eid + ".txt";
    {
        std::ifstream test(end_path);
        if (test.is_open()) return;
    }

    // event_date_str vem no formato "dd-mm-yyyy hh:mm"
    int day=0, month=0, year=0, hour=0, min=0;
    if (std::sscanf(event_date_str.c_str(), "%2d-%2d-%4d %2d:%2d",
                    &day, &month, &year, &hour, &min) != 5) {
        return;
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
        return;
    }

    std::ofstream out(end_path);
    if (!out.is_open()) return;

    out << buf << "\n";
}



// FUNÇÃO PRINCIPAL
ReserveStatus es_make_reservation(const std::string &uid,
                                  const std::string &pass,
                                  const std::string &eid,
                                  int people,
                                  int &remaining_out)
{
    remaining_out = 0;

    // ---------- Validar utilizador ----------
    // Enunciado RID/RRI:
    //  - se user não está logged in -> NLG
    //  - se password incorreta -> WRP
    // Não fala explicitamente em "user não existe", podemos tratá-lo
    // também como "não logged in" para não revelar info.
    if (!es_user_exists(uid)) {
        return ReserveStatus::NLG;
    }

    if (!es_user_is_logged_in(uid)) {
        return ReserveStatus::NLG;
    }

    if (!es_user_check_password(uid, pass)) {
        return ReserveStatus::WRP;
    }

    // ---------- Carregar evento ----------
    EventInfo ev;
    if (!load_event(eid, ev)) {
        // evento não existe ou START mal formado
        return ReserveStatus::NOK;
    }

    // ---------- Verificar estado ----------
    // Se o evento já for passado, podemos ainda criar o END se não existir,
    // para ficar coerente com o guia (mas já devolvemos PST).
    if (ev.state == EventState::Past) {
        maybe_create_end_for_past_event(eid, ev.event_date);
        return ReserveStatus::PST;
    }

    if (ev.state == EventState::ClosedByUser) {
        return ReserveStatus::CLS;
    }

    if (ev.state == EventState::SoldOut) {
        return ReserveStatus::SLD;
    }

    // Só continuamos se estiver OPEN
    // (EventState::Open)

    // Disponibilidade 
    int total_capacity = ev.capacity;
    int total_reserved = ev.reserved;
    int remaining = total_capacity - total_reserved;

    if (remaining <= 0) {
        return ReserveStatus::SLD;
    }

    if (people > remaining) {
        // rejeitada mas devolvemos remaining
        remaining_out = remaining;
        return ReserveStatus::REJ;
    }

    // ---------- Podem reservar ----------
    int new_total = total_reserved + people;

    // actualizar RES EID.txt
    if (!write_int_file(res_file(eid), new_total)) {
        return ReserveStatus::NOK;
    }

    // Gerar nomes para ficheiros de reserva
    std::string filename;        // R-UID-YYYY-MM-DD HHMMSS.txt
    std::string datetime_str;    // DD-MM-YYYY HH:MM:SS
    make_reservation_names(uid, filename, datetime_str);
    if (filename.empty()) {
        return ReserveStatus::NOK;
    }

    // Garantir que diretórios RESERVATIONS e RESERVED existem
    std::error_code ec;
    fs::create_directories(event_dir(eid) + "/RESERVATIONS", ec);
    fs::create_directories("USERS/" + uid + "/RESERVED", ec);

    // Ficheiro EVENTS/eid/RESERVATIONS/R-uid-date time.txt
    {
        std::string path = event_dir(eid) + "/RESERVATIONS/" + filename;
        std::ofstream out(path);
        if (!out.is_open())
            return ReserveStatus::NOK;

        // Conteúdo: UID res_num res_datetime
        out << uid << " " << people << " " << datetime_str << "\n";
        if (!out.good())
            return ReserveStatus::NOK;
    }

    // Ficheiro USERS/uid/RESERVED/R-uid-date time.txt (cópia)
    {
        std::string path = "USERS/" + uid + "/RESERVED/" + filename;
        std::ofstream out(path);
        if (!out.is_open())
            return ReserveStatus::NOK;

        out << uid << " " << people << " " << datetime_str << "\n";
        if (!out.good())
            return ReserveStatus::NOK;
    }

    return ReserveStatus::ACC;
}

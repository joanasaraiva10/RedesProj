#pragma once
#include <string>
#include <cstdint>
#include <ctime>


// Constantes do protocolo
constexpr int UID_LEN              = 6;          // 6 dígitos
constexpr int PASSWORD_LEN         = 8;          // 8 alfanum
constexpr int EVENT_NAME_MAX       = 10;         // até 10 chars
constexpr int FNAME_MAX            = 24;         // até 24 chars
constexpr int MIN_ATTENDANCE       = 10;         // 10..999
constexpr int MAX_ATTENDANCE       = 999;
constexpr int MAX_RESERVE_PEOPLE   = 999;        // 1..999
constexpr int MAX_FILE_SIZE_BYTES  = 10'000'000; // 10 MB

// Funções de validação

// UID: exatamente 6 dígitos
bool proto_valid_uid(const std::string &uid);

// Password: exatamente 8 caracteres alfanuméricos
bool proto_valid_password(const std::string &pass);

// Nome de evento: 1..10 caracteres alfanuméricos
bool proto_valid_event_name(const std::string &name);

// Nome de ficheiro: 1..24 chars, [A-Za-z0-9._-], extensão .xxx (3 letras)
bool proto_valid_fname(const std::string &fname);

// EID: exatamente 3 dígitos
bool proto_valid_eid(const std::string &eid);

// Data: "dd-mm-yyyy"
bool proto_valid_date_ddmmyyyy(const std::string &date);

// Hora: "hh:mm"
bool proto_valid_time_hhmm(const std::string &time);

// Datetime com segundos: "dd-mm-yyyy hh:mm:ss"
bool proto_valid_datetime_with_seconds(const std::string &dt);

// Parse de datetime com segundos (retorna 0 em caso de erro)
// Formato esperado: "dd-mm-yyyy hh:mm:ss"
std::time_t proto_parse_datetime_with_seconds(const std::string &dt);

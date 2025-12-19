// udp_handler.cpp
#include "udp_handler.h"
#include "udp_client.h"

#include <iostream>
#include <sstream>
#include <string>


//  LOGIN 
static void udp_handle_login(ClientState *state,
                             const ClientNetConfig *cfg,
                             const char *line)
{
    if (state->logged_in) {
        std::cout << "Already logged in as " << state->uid
                  << ". Please logout first.\n";
        return;
    }

    std::string cmd, uid, pass;
    {
        std::istringstream iss(line);
        iss >> cmd >> uid >> pass;
    }

    if (cmd != "login" || uid.empty() || pass.empty()) {
        std::cerr << "Usage: login <UID> <password>\n";
        return;
    }

    std::string request = "LIN " + uid + " " + pass + "\n";

    std::string response;
    if (udp_send_and_receive(cfg, request, response) != 0) {
        std::cerr << "Failed to communicate with server via UDP.\n";
        return;
    }

    std::istringstream iss(response);
    std::string tag, status;
    iss >> tag >> status;

    if (tag != "RLI" || status.empty()) {
        std::cerr << "Protocol error on login: " << response << "\n";
        return;
    }

    if (status == "OK" || status == "REG") {
        state->uid       = uid;
        state->pass      = pass;
        state->logged_in = true;

        if (status == "OK") {
            std::cout << "Login successful. Logged in as " << uid << ".\n";
        } else {
            std::cout << "New user registered. Logged in as " << uid << ".\n";
        }

    } else if (status == "NOK") {
        std::cout << "Login failed: wrong password.\n";
    } else {
        std::cout << "Login failed: " << status << "\n";
    }
}


//  LOGOUT 
static void udp_handle_logout(ClientState *state,
                              const ClientNetConfig *cfg,
                              const char *line)
{
    if (!state->logged_in) {
        std::cout << "User not logged in.\n";
        return;
    }

    std::string cmd;
    {
        std::istringstream iss(line);
        iss >> cmd;
    }

    if (cmd != "logout") {
        std::cerr << "Usage: logout\n";
        return;
    }

    std::string request = "LOU " + state->uid + " " + state->pass + "\n";

    std::string response;
    if (udp_send_and_receive(cfg, request, response) != 0) {
        std::cerr << "Failed to communicate with server via UDP.\n";
        return;
    }

    std::istringstream iss(response);
    std::string tag, status;
    iss >> tag >> status;

    if (tag != "RLO" || status.empty()) {
        std::cerr << "Protocol error on logout: " << response << "\n";
        return;
    }

    if (status == "OK") {
        state->logged_in = false;
        state->uid.clear();
        state->pass.clear();

    } else if (status == "NOK") {
        std::cout << "User not logged in (server side mismatch).\n";
        state->logged_in = false;
        state->uid.clear();
        state->pass.clear();

    } else if (status == "UNR") {
        std::cout << "Unknown user.\n";
        state->logged_in = false;
        state->uid.clear();
        state->pass.clear();

    } else if (status == "WRP") {
        std::cout << "Incorrect password.\n";
    } else {
        std::cout << "Logout failed: " << status << "\n";
    }
}


//  UNREGISTER 
static void udp_handle_unregister(ClientState *state,
                                  const ClientNetConfig *cfg,
                                  const char *line)
{
    if (!state->logged_in) {
        std::cout << "User not logged in.\n";
        return;
    }

    std::string cmd;
    {
        std::istringstream iss(line);
        iss >> cmd;
    }

    if (cmd != "unregister") {
        std::cerr << "Usage: unregister\n";
        return;
    }

    std::string request = "UNR " + state->uid + " " + state->pass + "\n";

    std::string response;
    if (udp_send_and_receive(cfg, request, response) != 0) {
        std::cerr << "Failed to communicate with server via UDP.\n";
        return;
    }

    std::istringstream iss(response);
    std::string tag, status;
    iss >> tag >> status;

    if (tag != "RUR" || status.empty()) {
        std::cerr << "Protocol error on unregister: " << response << "\n";
        return;
    }

    if (status == "OK") {
        std::cout << "User unregistered successfully.\n";
        state->logged_in = false;
        state->uid.clear();
        state->pass.clear();

    } else if (status == "NOK") {
        std::cout << "Unregister not allowed (server reports user not logged in).\n";
        state->logged_in = false;
        state->uid.clear();
        state->pass.clear();

    } else if (status == "UNR") {
        std::cout << "Unknown user.\n";
        state->logged_in = false;
        state->uid.clear();
        state->pass.clear();

    } else if (status == "WRP") {
        std::cout << "Incorrect password.\n";
    } else {
        std::cout << "Unregister failed: " << status << "\n";
    }
}


//  MYRESERVATIONS 
static void udp_handle_myreservations(ClientState *state,
                                      const ClientNetConfig *cfg,
                                      const char *line)
{
    if (!state->logged_in) {
        std::cout << "User not logged in.\n";
        return;
    }

    std::string cmd;
    {
        std::istringstream iss(line);
        iss >> cmd;
    }

    if (cmd != "myreservations" && cmd != "myr" && cmd != "myres") {
        std::cerr << "Usage: myreservations\n";
        return;
    }

    std::string request = "LMR " + state->uid + " " + state->pass + "\n";

    std::string response;
    if (udp_send_and_receive(cfg, request, response) != 0) {
        std::cerr << "Failed to communicate with server via UDP.\n";
        return;
    }

    std::istringstream iss(response);
    std::string tag, status;
    iss >> tag >> status;

    if (tag != "RMR" || status.empty()) {
        std::cerr << "Protocol error on myreservations: " << response << "\n";
        return;
    }

    if (status == "NOK") {
        std::cout << "You have not made any reservations.\n";
        return;
    }
    if (status == "NLG") {
        std::cout << "User not logged in (server side).\n";
        state->logged_in = false;
        return;
    }
    if (status == "WRP") {
        std::cout << "Incorrect password.\n";
        return;
    }
    if (status == "ERR") {
        std::cout << "Error processing myreservations request.\n";
        return;
    }
    if (status != "OK") {
        std::cout << "Unknown status in myreservations reply: " << status << "\n";
        return;
    }

    std::cout << "Reservations made by user " << state->uid << ":\n";

    std::string eid, date, time, valStr;
    int count = 0;

    while (iss >> eid >> date >> time >> valStr) {
        int value = std::stoi(valStr);
        std::cout << "  Event " << eid
                  << " on " << date << " " << time
                  << " -> " << value << " places reserved\n";
        ++count;
    }

    if (count == 0) {
        std::cout << "No reservations returned by server.\n";
    }
}


//  MYEVENTS 
static void udp_handle_myevents(ClientState *state,
                                const ClientNetConfig *cfg,
                                const char *line)
{
    if (!state->logged_in) {
        std::cout << "User not logged in.\n";
        return;
    }

    std::string cmd;
    {
        std::istringstream iss(line);
        iss >> cmd;
    }

    if (cmd != "myevents" && cmd != "mye") {
        std::cerr << "Usage: myevents\n";
        return;
    }

    std::string request = "LME " + state->uid + " " + state->pass + "\n";

    std::string response;
    if (udp_send_and_receive(cfg, request, response) != 0) {
        std::cerr << "Failed to communicate with server via UDP.\n";
        return;
    }

    std::istringstream iss(response);
    std::string tag, status;
    iss >> tag >> status;

    if (tag != "RME" || status.empty()) {
        std::cerr << "Protocol error on myevents: " << response << "\n";
        return;
    }

    if (status == "NOK") {
        std::cout << "You have not created any events.\n";
        return;
    }
    if (status == "NLG") {
        std::cout << "User not logged in (server side).\n";
        state->logged_in = false;
        return;
    }
    if (status == "WRP") {
        std::cout << "Incorrect password.\n";
        return;
    }
    if (status == "ERR") {
        std::cout << "Error processing myevents request.\n";
        return;
    }
    if (status != "OK") {
        std::cout << "Unknown status in myevents reply: " << status << "\n";
        return;
    }

    std::cout << "Events created by user " << state->uid << ":\n";

    std::string eid, stStr;
    int count = 0;

    while (iss >> eid >> stStr) {
        int s = std::stoi(stStr);
        const char *desc;
        switch (s) {
            case 1: desc = "accepting reservations"; break;
            case 0: desc = "in the past";            break;
            case 2: desc = "sold out";               break;
            case 3: desc = "closed by user";         break;
            default: desc = "unknown state";         break;
        }

        std::cout << "  Event " << eid << " -> state " << s
                  << " (" << desc << ")\n";
        ++count;
    }

    if (count == 0) {
        std::cout << "No events returned by server.\n";
    }
}


//  DISPATCHER UDP
void udp_dispatch_command(ClientState *state,
                          const ClientNetConfig *cfg,
                          const char *line)
{
    std::string cmd;
    {
        std::istringstream iss(line);
        iss >> cmd;
    }

    if (cmd.empty()) {
        std::cerr << "Empty command line.\n";
        return;
    }

    if (cmd == "login") {
        udp_handle_login(state, cfg, line);
    } else if (cmd == "logout") {
        udp_handle_logout(state, cfg, line);
    } else if (cmd == "unregister") {
        udp_handle_unregister(state, cfg, line);
    } else if (cmd == "myevents" || cmd == "mye") {
        udp_handle_myevents(state, cfg, line);
    } else if (cmd == "myreservations" ||
               cmd == "myr" || cmd == "myres") {
        udp_handle_myreservations(state, cfg, line);
    } else {
        std::cout << "Unknown UDP command: " << cmd << "\n";
    }
}

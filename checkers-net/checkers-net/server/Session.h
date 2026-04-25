#pragma once
// =============================================================================
// Session.h  —  Everything the server remembers about one connected client
// =============================================================================
#include <string>
#include <atomic>

struct Session {
    std::string sessionId;   // Unique token sent to client on WELCOME
    std::string name;        // Player's chosen display name
    std::string roomId;      // Which room this session belongs to ("" if none)
    int         playerNum;   // 1, 2, or 0 for spectator
    int         fd;          // Current socket file descriptor (-1 = disconnected)
    std::atomic<bool> connected{false};

    Session() : playerNum(0), fd(-1) {}
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
};

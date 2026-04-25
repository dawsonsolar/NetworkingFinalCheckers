#pragma once
// Stores per-client state on the server. Persists through disconnects
// so players can reconnect to an in-progress game using their session ID.
#include <string>
#include <atomic>

struct Session {
    std::string sessionId;
    std::string name;
    std::string roomId;
    int         playerNum = 0;   // 1, 2, or 0 for spectator
    int         fd        = -1;  // socket fd, -1 when disconnected
    std::atomic<bool> connected{false};

    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
};

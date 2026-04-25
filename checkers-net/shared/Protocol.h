#pragma once
// Newline-terminated, pipe-delimited messages sent over TCP.
// e.g. "MOVE|5|0|4|1\n" or "STATE|board64|turn|p1|p2\n"

#include <string>
#include <vector>
#include <sstream>

namespace Protocol {

    static const char SEP = '|';

    // Client -> Server
    static const std::string CONNECT     = "CONNECT";
    static const std::string RECONNECT   = "RECONNECT";
    static const std::string MOVE        = "MOVE";
    static const std::string SPECTATE    = "SPECTATE";

    // Server -> Client
    static const std::string WELCOME     = "WELCOME";
    static const std::string WAITING     = "WAITING";
    static const std::string START       = "START";
    static const std::string STATE       = "STATE";
    static const std::string YOUR_TURN   = "YOUR_TURN";
    static const std::string WAIT_TURN   = "WAIT_TURN";
    static const std::string GAME_OVER   = "GAME_OVER";
    static const std::string OPPONENT_DC = "OPPONENT_DC";
    static const std::string REJOINED    = "REJOINED";
    static const std::string MSG         = "MSG";
    static const std::string ERR         = "ERR";

    // Split a raw message on '|'
    inline std::vector<std::string> split(const std::string& s)
    {
        std::vector<std::string> tokens;
        std::stringstream ss(s);
        std::string token;
        while (std::getline(ss, token, SEP)) tokens.push_back(token);
        return tokens;
    }

    // Build a pipe-delimited message with a trailing newline
    inline std::string build(std::initializer_list<std::string> parts)
    {
        std::string msg;
        bool first = true;
        for (const auto& p : parts)
        {
            if (!first) msg += SEP;
            msg += p;
            first = false;
        }
        return msg + '\n';
    }

    inline std::string build(const std::string& type)
    {
        return type + '\n';
    }

} // namespace Protocol

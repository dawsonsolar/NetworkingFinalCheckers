#pragma once
// =============================================================================
// Protocol.h  —  Newline-terminated, pipe-delimited text messages
// =============================================================================
// Client → Server:
//   CONNECT|name
//   RECONNECT|session_id
//   MOVE|fromRow|fromCol|toRow|toCol
//   SPECTATE                        (join next available room as observer)
//
// Server → Client:
//   WELCOME|session_id|player_num|room_id
//   WAITING                         (no opponent yet)
//   START|opponent_name
//   STATE|board64|turn|p1name|p2name
//   YOUR_TURN
//   WAIT_TURN
//   GAME_OVER|winner_num|reason
//   OPPONENT_DC
//   REJOINED|player_num|board64|turn|p1name|p2name
//   MSG|text
//   ERR|reason
// =============================================================================

#include <string>
#include <vector>
#include <sstream>

namespace Protocol {

    static const char SEP = '|';

    // ── Message type constants ─────────────────────────────────────────────
    // Client → Server
    static const std::string CONNECT     = "CONNECT";
    static const std::string RECONNECT   = "RECONNECT";
    static const std::string MOVE        = "MOVE";
    static const std::string SPECTATE    = "SPECTATE";

    // Server → Client
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

    // ── Helpers ───────────────────────────────────────────────────────────

    // Split a raw message string on '|'
    inline std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> tokens;
        std::stringstream ss(s);
        std::string token;
        while (std::getline(ss, token, SEP)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    // Build a message from parts, appending '\n'
    inline std::string build(std::initializer_list<std::string> parts) {
        std::string msg;
        bool first = true;
        for (const auto& p : parts) {
            if (!first) msg += SEP;
            msg += p;
            first = false;
        }
        msg += '\n';
        return msg;
    }

    // Convenience: build a no-arg message
    inline std::string build(const std::string& type) {
        return type + '\n';
    }

} // namespace Protocol

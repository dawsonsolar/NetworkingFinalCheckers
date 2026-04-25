#pragma once
// =============================================================================
// GameServer.h  —  TCP listener + session manager + room broker
// =============================================================================
// Architecture:
//   • Main thread: accept() loop
//   • Per-client thread: reads newline-delimited messages, dispatches
//   • GameRoom: shared state protected by its own mutex
//   • Sessions stored by sessionId for reconnection support
// =============================================================================
#include "Session.h"
#include "GameRoom.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>

class GameServer {
public:
    explicit GameServer(int port);
    void run();   // Blocks forever (call from main)

private:
    // ── Client thread entry point ──────────────────────────────────────────
    void handleClient(int fd);

    // ── Message handlers ───────────────────────────────────────────────────
    void onConnect   (int fd, const std::vector<std::string>& parts);
    void onReconnect (int fd, const std::vector<std::string>& parts);
    void onMove      (Session* s, const std::vector<std::string>& parts);
    void onSpectate  (Session* s);
    void onDisconnect(Session* s);

    // ── Room helpers ───────────────────────────────────────────────────────
    GameRoom* findOrCreateRoom();   // Returns a room that needs a player
    GameRoom* roomById(const std::string& id);

    // ── Utility ────────────────────────────────────────────────────────────
    static std::string makeSessionId();
    static void sendTo(int fd, const std::string& msg);
    static std::string readLine(int fd);

    // ── State ──────────────────────────────────────────────────────────────
    int port_;
    int listenFd_ = -1;

    // sessionId → Session  (owner)
    std::unordered_map<std::string, std::unique_ptr<Session>> sessions_;
    std::mutex sessionsMtx_;

    // roomId → GameRoom  (owner)
    std::unordered_map<std::string, std::unique_ptr<GameRoom>> rooms_;
    std::mutex roomsMtx_;

    std::atomic<int> roomCounter_{0};
};

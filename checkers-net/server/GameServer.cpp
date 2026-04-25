#include "GameServer.h"
#include "../shared/Protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <cstring>

GameServer::GameServer(int port) : port_(port) {}

void GameServer::run() {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) { perror("socket"); return; }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));

    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind"); return;
    }
    if (listen(listenFd_, 16) < 0) { perror("listen"); return; }

    std::cout << "[Server] Listening on port " << port_ << "\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        int cfd = accept(listenFd_,
                         reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (cfd < 0) { perror("accept"); continue; }

        std::cout << "[Server] New connection fd=" << cfd
                  << " from " << inet_ntoa(clientAddr.sin_addr) << "\n";

        // Spawn a detached thread for this client
        std::thread([this, cfd]() { handleClient(cfd); }).detach();
    }
}

// Per-client thread

void GameServer::handleClient(int fd) {
    // First message must be CONNECT or RECONNECT
    std::string firstLine = readLine(fd);
    if (firstLine.empty()) { close(fd); return; }

    auto parts = Protocol::split(firstLine);
    Session* session = nullptr;

    if (!parts.empty() && parts[0] == Protocol::CONNECT) {
        onConnect(fd, parts);
        // Look up the session we just created by fd
        std::lock_guard<std::mutex> lk(sessionsMtx_);
        for (auto& [id, s] : sessions_) {
            if (s->fd == fd) { session = s.get(); break; }
        }
    } else if (!parts.empty() && parts[0] == Protocol::RECONNECT) {
        onReconnect(fd, parts);
        std::lock_guard<std::mutex> lk(sessionsMtx_);
        if (parts.size() > 1) {
            auto it = sessions_.find(parts[1]);
            if (it != sessions_.end()) session = it->second.get();
        }
    } else {
        sendTo(fd, Protocol::build({Protocol::ERR, "First message must be CONNECT or RECONNECT"}));
        close(fd);
        return;
    }

    if (!session) { close(fd); return; }

    // Main read loop for this client
    while (true) {
        std::string line = readLine(fd);
        if (line.empty()) break;  // Disconnected or error

        auto p = Protocol::split(line);
        if (p.empty()) continue;

        if (p[0] == Protocol::MOVE) {
            onMove(session, p);
        } else if (p[0] == Protocol::SPECTATE) {
            onSpectate(session);
        }
        // Ignore unknown message types gracefully
    }

    onDisconnect(session);
}

// Message handlers

void GameServer::onConnect(int fd, const std::vector<std::string>& parts) {
    std::string name = (parts.size() > 1 && !parts[1].empty())
                       ? parts[1] : "Player";

    // Create session
    auto sess       = std::make_unique<Session>();
    sess->sessionId = makeSessionId();
    sess->name      = name;
    sess->fd        = fd;
    sess->connected = true;
    Session* s      = sess.get();

    {
        std::lock_guard<std::mutex> lk(sessionsMtx_);
        sessions_[sess->sessionId] = std::move(sess);
    }

    // Find or create a room
    GameRoom* room = findOrCreateRoom();
    room->addPlayer(s);

    sendTo(fd, Protocol::build({Protocol::WELCOME, s->sessionId,
                                 std::to_string(s->playerNum), room->id()}));

    std::cout << "[Server] " << name << " joined room " << room->id()
              << " as P" << s->playerNum << "\n";

    if (!room->isFull()) {
        sendTo(fd, Protocol::build(Protocol::WAITING));
    }
    // If room is full, startGame() fires inside addPlayer() which sends START+STATE
}

void GameServer::onReconnect(int fd, const std::vector<std::string>& parts) {
    if (parts.size() < 2) {
        sendTo(fd, Protocol::build({Protocol::ERR, "Missing session_id"}));
        close(fd);
        return;
    }
    const std::string& sid = parts[1];

    Session* s = nullptr;
    {
        std::lock_guard<std::mutex> lk(sessionsMtx_);
        auto it = sessions_.find(sid);
        if (it == sessions_.end()) {
            sendTo(fd, Protocol::build({Protocol::ERR, "Unknown session"}));
            close(fd);
            return;
        }
        s = it->second.get();
    }

    // Re-attach the socket
    s->fd        = fd;
    s->connected = true;

    // Find their room and resend state
    GameRoom* room = nullptr;
    {
        std::lock_guard<std::mutex> lk(roomsMtx_);
        auto it = rooms_.find(s->roomId);
        if (it != rooms_.end()) room = it->second.get();
    }

    sendTo(fd, Protocol::build({Protocol::REJOINED,
                                 std::to_string(s->playerNum),
                                 room ? room->statePayload() : ""}));

    std::cout << "[Server] " << s->name << " reconnected to room " << s->roomId << "\n";
}

void GameServer::onMove(Session* s, const std::vector<std::string>& parts) {
    if (parts.size() < 5) {
        sendTo(s->fd, Protocol::build({Protocol::ERR, "MOVE requires 4 coordinates"}));
        return;
    }
    int fr = std::stoi(parts[1]), fc = std::stoi(parts[2]);
    int tr = std::stoi(parts[3]), tc = std::stoi(parts[4]);

    GameRoom* room = nullptr;
    {
        std::lock_guard<std::mutex> lk(roomsMtx_);
        auto it = rooms_.find(s->roomId);
        if (it != rooms_.end()) room = it->second.get();
    }
    if (!room) {
        sendTo(s->fd, Protocol::build({Protocol::ERR, "Not in a room"}));
        return;
    }
    if (!room->onMove(s, fr, fc, tr, tc)) {
        sendTo(s->fd, Protocol::build({Protocol::ERR, "Illegal move"}));
    }
}

void GameServer::onSpectate(Session* s) {
    GameRoom* room = nullptr;
    {
        std::lock_guard<std::mutex> lk(roomsMtx_);
        // Find any started room
        for (auto& [id, r] : rooms_) {
            if (r->isStarted()) { room = r.get(); break; }
        }
        if (!room && !rooms_.empty()) {
            room = rooms_.begin()->second.get();
        }
    }
    if (room) room->addSpectator(s);
    else sendTo(s->fd, Protocol::build({Protocol::ERR, "No rooms available to spectate"}));
}

void GameServer::onDisconnect(Session* s) {
    if (!s) return;
    std::cout << "[Server] " << s->name << " disconnected\n";
    s->connected = false;
    s->fd        = -1;

    // Notify the room (does NOT delete the session — reconnection window stays open)
    GameRoom* room = nullptr;
    {
        std::lock_guard<std::mutex> lk(roomsMtx_);
        auto it = rooms_.find(s->roomId);
        if (it != rooms_.end()) room = it->second.get();
    }
    if (room) room->removeClient(s);
}

// Room helpers

GameRoom* GameServer::findOrCreateRoom() {
    std::lock_guard<std::mutex> lk(roomsMtx_);
    // Look for an existing room waiting for a second player
    for (auto& [id, room] : rooms_) {
        if (!room->isFull() && !room->isStarted()) return room.get();
    }
    // Create a new room
    std::string rid = "R" + std::to_string(++roomCounter_);
    auto room       = std::make_unique<GameRoom>(rid);
    GameRoom* ptr   = room.get();
    rooms_[rid]     = std::move(room);
    std::cout << "[Server] Created room " << rid << "\n";
    return ptr;
}

GameRoom* GameServer::roomById(const std::string& id) {
    std::lock_guard<std::mutex> lk(roomsMtx_);
    auto it = rooms_.find(id);
    return (it != rooms_.end()) ? it->second.get() : nullptr;
}

// Utility

std::string GameServer::makeSessionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);
    std::ostringstream ss;
    for (int i = 0; i < 4; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    return ss.str();
}

void GameServer::sendTo(int fd, const std::string& msg) {
    if (fd < 0) return;
    ::send(fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
}

// Read one newline-terminated line from fd
std::string GameServer::readLine(int fd) {
    std::string result;
    char c;
    while (true) {
        int n = recv(fd, &c, 1, 0);
        if (n <= 0) return "";   // Disconnected or error
        if (c == '\n') return result;
        if (c != '\r') result += c;
        if (result.size() > 4096) return ""; // Guard against runaway input
    }
}

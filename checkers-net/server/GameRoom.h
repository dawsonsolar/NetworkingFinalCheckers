#pragma once
// =============================================================================
// GameRoom.h  —  One game instance: 2 players + any number of spectators
// =============================================================================
#include "../shared/CheckersLogic.h"
#include "Session.h"
#include <string>
#include <vector>
#include <mutex>
#include <functional>

class GameRoom {
public:
    explicit GameRoom(std::string id);

    const std::string& id() const { return roomId_; }

    // Returns true if the room accepted the player (needs exactly 2 to start)
    bool addPlayer   (Session* s);
    void addSpectator(Session* s);
    void removeClient(Session* s);   // Called on disconnect

    // Process a MOVE message from a client; returns false if illegal / wrong turn
    bool onMove(Session* sender, int fr, int fc, int tr, int tc);

    bool isFull()     const;   // both player slots taken
    bool isStarted()  const;   // game has begun
    bool isEmpty()    const;   // no players remaining
    void notifyReconnect(Session* s); // notify opponent + resend turn state

    // Formatted state string (board64|turn|p1name|p2name)
    std::string statePayload() const;

private:
    void broadcastAll (const std::string& msg);
    void broadcastPlayers(const std::string& msg);
    void send(Session* s, const std::string& msg);
    void startGame();
    void sendState(Session* s = nullptr);  // nullptr = broadcast to all
    int  opponent(int playerNum) const;

    std::string  roomId_;
    Session*     players_[2] = {nullptr, nullptr};  // index 0 = P1, 1 = P2
    std::vector<Session*> spectators_;
    GameState    state_;
    bool         started_ = false;
    mutable std::mutex mtx_;
};

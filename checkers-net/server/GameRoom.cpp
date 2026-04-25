#include "GameRoom.h"
#include "../shared/Protocol.h"
#include <sys/socket.h>
#include <cstring>
#include <iostream>

GameRoom::GameRoom(std::string id) : roomId_(std::move(id)) {}

// Client management

bool GameRoom::addPlayer(Session* s) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (int i = 0; i < 2; i++) {
        if (players_[i] == nullptr) {
            players_[i]     = s;
            s->playerNum    = i + 1;
            s->roomId       = roomId_;
            if (players_[0] && players_[1]) {
                startGame(); // Both seats filled — begin immediately
            }
            return true;
        }
    }
    return false; // Room already full
}

void GameRoom::addSpectator(Session* s) {
    std::lock_guard<std::mutex> lk(mtx_);
    s->playerNum = 0;
    s->roomId    = roomId_;
    spectators_.push_back(s);
    send(s, Protocol::build({Protocol::MSG, "Joined as spectator for room " + roomId_}));
    if (started_) {
        send(s, Protocol::build({Protocol::STATE, statePayload()}));
    }
}

void GameRoom::removeClient(Session* s) {
    std::lock_guard<std::mutex> lk(mtx_);
    // Remove from spectators list if present
    spectators_.erase(std::remove(spectators_.begin(), spectators_.end(), s),
                      spectators_.end());
    // Notify opponent if a player left mid-game
    for (int i = 0; i < 2; i++) {
        if (players_[i] == s) {
            players_[i] = nullptr;
            int oppIdx  = 1 - i;
            if (started_ && players_[oppIdx] && players_[oppIdx]->fd >= 0) {
                send(players_[oppIdx],
                     Protocol::build(Protocol::OPPONENT_DC));
            }
            break;
        }
    }
}

// Move handling

bool GameRoom::onMove(Session* sender, int fr, int fc, int tr, int tc) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!started_ || state_.gameOver) return false;
    if (sender->playerNum != state_.currentPlayer) return false;

    Move m;
    m.fromRow = fr; m.fromCol = fc;
    m.toRow   = tr; m.toCol   = tc;

    // Attempt to apply — CheckersLogic fills in capture fields via validMoves lookup
    // We need to find the matching legal move to copy capture info
    auto legal = CheckersLogic::validMoves(state_);
    bool found = false;
    for (const auto& v : legal) {
        if (v.fromRow == fr && v.fromCol == fc && v.toRow == tr && v.toCol == tc) {
            m = v;   // Use validated move with capture fields filled in
            found = true;
            break;
        }
    }
    if (!found) return false;

    if (!CheckersLogic::applyMove(state_, m)) return false;

    // Broadcast updated state to everyone
    std::string stateMsg = Protocol::build({Protocol::STATE, statePayload()});
    broadcastAll(stateMsg);

    if (state_.gameOver) {
        std::string winner = (state_.winner == 1)
            ? (players_[0] ? players_[0]->name : "P1")
            : (players_[1] ? players_[1]->name : "P2");
        broadcastAll(Protocol::build({Protocol::GAME_OVER,
                                      std::to_string(state_.winner),
                                      winner + " wins!"}));
    } else {
        // Tell each player whether it is their turn
        for (int i = 0; i < 2; i++) {
            if (!players_[i] || players_[i]->fd < 0) continue;
            bool myTurn = (state_.currentPlayer == i + 1);
            send(players_[i], Protocol::build(myTurn ? Protocol::YOUR_TURN
                                                      : Protocol::WAIT_TURN));
        }
    }
    return true;
}

// State payload

std::string GameRoom::statePayload() const {
    // board64|turn|p1name|p2name
    std::string p1name = players_[0] ? players_[0]->name : "?";
    std::string p2name = players_[1] ? players_[1]->name : "?";
    return CheckersLogic::boardToString(state_.board)
         + Protocol::SEP + std::to_string(state_.currentPlayer)
         + Protocol::SEP + p1name
         + Protocol::SEP + p2name;
}

// Queries

bool GameRoom::isFull()    const { return players_[0] && players_[1]; }
bool GameRoom::isStarted() const { return started_; }
bool GameRoom::isEmpty()   const { return !players_[0] && !players_[1] && spectators_.empty(); }

// Private helpers

void GameRoom::send(Session* s, const std::string& msg) {
    if (!s || s->fd < 0) return;
    ::send(s->fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
}

void GameRoom::broadcastAll(const std::string& msg) {
    for (auto* p : players_) send(p, msg);
    for (auto* sp : spectators_) send(sp, msg);
}

void GameRoom::broadcastPlayers(const std::string& msg) {
    for (auto* p : players_) send(p, msg);
}

void GameRoom::startGame() {
    // Called while mtx_ already held
    state_   = CheckersLogic::initialState();
    started_ = true;
    std::cout << "[Room " << roomId_ << "] Game started: "
              << players_[0]->name << " vs " << players_[1]->name << "\n";

    // Tell each player who they are and who their opponent is
    for (int i = 0; i < 2; i++) {
        int oppIdx = 1 - i;
        send(players_[i],
             Protocol::build({Protocol::START, players_[oppIdx]->name}));
    }

    // Send initial board state
    std::string stateMsg = Protocol::build({Protocol::STATE, statePayload()});
    broadcastAll(stateMsg);

    // Tell P1 it's their turn
    send(players_[0], Protocol::build(Protocol::YOUR_TURN));
    send(players_[1], Protocol::build(Protocol::WAIT_TURN));
}

// =============================================================================
// client/main.cpp  —  Terminal client for Networked Checkers
// =============================================================================
// Input:  row col row col  (e.g. "5 0 4 1")
// During multi-jump the same prompt stays active until the turn ends.
// 'q' to quit.  On disconnect, enter session ID to reconnect.
// =============================================================================
#include "../shared/Protocol.h"
#include "../shared/CheckersLogic.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdlib>

// ── ANSI helpers ───────────────────────────────────────────────────────────
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BG_DARK "\033[48;5;238m"
#define BG_LITE "\033[48;5;250m"

// ── Client state ───────────────────────────────────────────────────────────
struct ClientState {
    int         sock       = -1;
    int         playerNum  = 0;   // 1 or 2
    std::string sessionId;
    std::string roomId;
    std::string myName;
    std::string p1Name, p2Name;
    Board       board;
    int         currentTurn = 1;
    bool        myTurn      = false;
    bool        gameOver    = false;
    std::string lastMsg;

    ClientState() {
        for (auto& row : board) row.fill(0);
    }
};

// ── Network helpers ────────────────────────────────────────────────────────

static void sendMsg(int sock, const std::string& msg) {
    send(sock, msg.c_str(), msg.size(), MSG_NOSIGNAL);
}

static std::string readLine(int sock) {
    std::string result;
    char c;
    while (true) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return "";
        if (c == '\n') return result;
        if (c != '\r') result += c;
        if (result.size() > 8192) return "";
    }
}

static int connectToServer(const std::string& host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << host << "\n";
        close(sock);
        return -1;
    }
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }
    return sock;
}

// ── Board rendering ────────────────────────────────────────────────────────

static void clearScreen() {
    std::cout << "\033[2J\033[H";
}

static void drawBoard(const ClientState& cs) {
    clearScreen();

    // Header
    std::cout << BOLD CYAN
              << "╔══════════════════════════════════════════════╗\n"
              << "║         NETWORKED CHECKERS  v1.0             ║\n"
              << "╚══════════════════════════════════════════════╝\n" RESET;

    std::cout << BOLD << "  Session: " << RESET << cs.sessionId
              << BOLD << "   Room: "   << RESET << cs.roomId << "\n";
    std::cout << BOLD << "  You: P"   << cs.playerNum
              << " (" << cs.myName << ")" << RESET << "\n";
    std::cout << BOLD << "  P1: " << RESET << cs.p1Name
              << "   " BOLD "P2: " RESET << cs.p2Name << "\n\n";

    // Column header
    std::cout << "     0    1    2    3    4    5    6    7\n";
    std::cout << "   ┌────┬────┬────┬────┬────┬────┬────┬────┐\n";

    for (int r = 0; r < 8; r++) {
        std::cout << " " << r << " │";
        for (int c = 0; c < 8; c++) {
            bool dark = (r + c) % 2 == 0;
            std::string bg = dark ? BG_DARK : BG_LITE;

            int cell = cs.board[r][c];
            std::string sym;
            if      (cell == 0) sym = "    ";
            else if (cell == 1) sym = std::string(" ") + GREEN  + BOLD + " ● " + RESET;
            else if (cell == 2) sym = std::string(" ") + RED    + BOLD + " ○ " + RESET;
            else if (cell == 3) sym = std::string(" ") + GREEN  + BOLD + " ♛ " + RESET; // P1 King
            else if (cell == 4) sym = std::string(" ") + RED    + BOLD + " ♛ " + RESET; // P2 King

            std::cout << bg;
            if (cell == 0) std::cout << "    ";
            else           std::cout << sym;
            std::cout << RESET << "│";
        }
        std::cout << "\n";
        if (r < 7)
            std::cout << "   ├────┼────┼────┼────┼────┼────┼────┼────┤\n";
    }
    std::cout << "   └────┴────┴────┴────┴────┴────┴────┴────┘\n\n";

    // Legend
    std::cout << "  " GREEN BOLD "●" RESET " = P1 (You)"
              << "   " RED BOLD "○" RESET " = P2"
              << "   " BOLD "♛" RESET " = King\n\n";

    // Turn / status
    if (cs.gameOver) {
        std::cout << BOLD YELLOW "  ★  GAME OVER  ★\n" RESET;
    } else if (cs.myTurn) {
        std::cout << BOLD GREEN "  ▶  YOUR TURN!\n" RESET;
        std::cout << "  Enter move (fromRow fromCol toRow toCol), or 'q' to quit:\n  > ";
        std::cout.flush();
    } else {
        std::string opponentName = (cs.playerNum == 1) ? cs.p2Name : cs.p1Name;
        std::cout << BOLD "  ⏳  Waiting for " << opponentName << "...\n" RESET;
    }

    if (!cs.lastMsg.empty()) {
        std::cout << "\n  " YELLOW "[" << cs.lastMsg << "]" RESET "\n";
    }
}

// ── Server message handler ─────────────────────────────────────────────────

static void processServerMessage(const std::string& line, ClientState& cs) {
    if (line.empty()) return;
    auto parts = Protocol::split(line);
    const std::string& type = parts[0];

    if (type == Protocol::WELCOME) {
        // WELCOME|session_id|player_num|room_id
        if (parts.size() >= 4) {
            cs.sessionId = parts[1];
            cs.playerNum = std::stoi(parts[2]);
            cs.roomId    = parts[3];
        }
        cs.lastMsg = "Connected! Session: " + cs.sessionId;
    }
    else if (type == Protocol::WAITING) {
        cs.lastMsg = "Waiting for an opponent to join room " + cs.roomId + "...";
        clearScreen();
        std::cout << BOLD CYAN "\n  Waiting for opponent..." RESET "\n"
                  << "  Session ID (save for reconnect): " BOLD << cs.sessionId << RESET "\n";
        std::cout.flush();
    }
    else if (type == Protocol::START) {
        // START|opponent_name
        std::string oppName = (parts.size() > 1) ? parts[1] : "Opponent";
        cs.lastMsg = "Game started vs " + oppName + "!";
    }
    else if (type == Protocol::STATE) {
        // STATE|board64|turn|p1name|p2name
        if (parts.size() >= 5) {
            cs.board       = CheckersLogic::boardFromString(parts[1]);
            cs.currentTurn = std::stoi(parts[2]);
            cs.p1Name      = parts[3];
            cs.p2Name      = parts[4];
        }
    }
    else if (type == Protocol::YOUR_TURN) {
        cs.myTurn  = true;
        cs.lastMsg = "";
        drawBoard(cs);
    }
    else if (type == Protocol::WAIT_TURN) {
        cs.myTurn  = false;
        drawBoard(cs);
    }
    else if (type == Protocol::GAME_OVER) {
        // GAME_OVER|winner_num|reason
        cs.gameOver = true;
        cs.myTurn   = false;
        cs.lastMsg  = (parts.size() > 2) ? parts[2] : "Game over";
        drawBoard(cs);
    }
    else if (type == Protocol::OPPONENT_DC) {
        cs.lastMsg = "Opponent disconnected. Waiting for them to return...";
        drawBoard(cs);
    }
    else if (type == Protocol::REJOINED) {
        // REJOINED|player_num|board64|turn|p1name|p2name
        if (parts.size() >= 6) {
            cs.playerNum   = std::stoi(parts[1]);
            cs.board       = CheckersLogic::boardFromString(parts[2]);
            cs.currentTurn = std::stoi(parts[3]);
            cs.p1Name      = parts[4];
            cs.p2Name      = parts[5];
        }
        cs.lastMsg = "Reconnected! Resuming game.";
        drawBoard(cs);
    }
    else if (type == Protocol::MSG) {
        cs.lastMsg = (parts.size() > 1) ? parts[1] : "";
        drawBoard(cs);
    }
    else if (type == Protocol::ERR) {
        cs.lastMsg = "Server error: " + (parts.size() > 1 ? parts[1] : "unknown");
        drawBoard(cs);
    }
}

// ── Input handler ──────────────────────────────────────────────────────────

static void processInput(const std::string& line, ClientState& cs) {
    if (line == "q" || line == "quit") {
        std::cout << "Goodbye!\n";
        close(cs.sock);
        exit(0);
    }
    if (!cs.myTurn) {
        cs.lastMsg = "It's not your turn.";
        drawBoard(cs);
        return;
    }
    // Parse: fromRow fromCol toRow toCol
    std::istringstream iss(line);
    int fr, fc, tr, tc;
    if (!(iss >> fr >> fc >> tr >> tc)) {
        cs.lastMsg = "Bad input. Format: fromRow fromCol toRow toCol";
        drawBoard(cs);
        return;
    }
    sendMsg(cs.sock,
            Protocol::build({Protocol::MOVE,
                              std::to_string(fr), std::to_string(fc),
                              std::to_string(tr), std::to_string(tc)}));
    // Server will reply with STATE + YOUR_TURN/WAIT_TURN
    cs.myTurn = false;
}

// ── Main ───────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int         port = 54000;
    if (argc > 1) host = argv[1];
    if (argc > 2) port = std::atoi(argv[2]);

    std::cout << BOLD CYAN "=== Networked Checkers Client ===" RESET "\n";
    std::cout << "Enter your name: ";
    std::string name;
    std::getline(std::cin, name);
    if (name.empty()) name = "Player";

    std::cout << "Reconnect with session ID? (leave blank for new game): ";
    std::string existingSession;
    std::getline(std::cin, existingSession);

    std::cout << "Connecting to " << host << ":" << port << "...\n";
    int sock = connectToServer(host, port);
    if (sock < 0) { std::cerr << "Could not connect.\n"; return 1; }

    ClientState cs;
    cs.sock   = sock;
    cs.myName = name;

    // Send CONNECT or RECONNECT
    if (existingSession.empty()) {
        sendMsg(sock, Protocol::build({Protocol::CONNECT, name}));
    } else {
        cs.sessionId = existingSession;
        sendMsg(sock, Protocol::build({Protocol::RECONNECT, existingSession}));
    }

    // ── Event loop ─────────────────────────────────────────────────────────
    // Uses select() to watch both the socket and stdin simultaneously,
    // so neither blocks the other.

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Only monitor stdin when it's our turn and game is active
        if (cs.myTurn && !cs.gameOver) {
            FD_SET(STDIN_FILENO, &readfds);
        }

        int maxFd = sock + 1;
        int ret   = select(maxFd, &readfds, nullptr, nullptr, nullptr);
        if (ret < 0) { perror("select"); break; }

        // ── Data from server ───────────────────────────────────────────────
        if (FD_ISSET(sock, &readfds)) {
            std::string line = readLine(sock);
            if (line.empty()) {
                // Server disconnected
                clearScreen();
                std::cout << RED BOLD "\n  Server disconnected.\n" RESET;
                std::cout << "  Your session ID was: " BOLD << cs.sessionId << RESET "\n";
                std::cout << "  Restart the client and enter it to reconnect.\n";
                break;
            }
            processServerMessage(line, cs);
        }

        // ── Data from stdin ────────────────────────────────────────────────
        if (cs.myTurn && !cs.gameOver && FD_ISSET(STDIN_FILENO, &readfds)) {
            std::string line;
            if (!std::getline(std::cin, line)) break;
            processInput(line, cs);
        }
    }

    close(sock);
    return 0;
}

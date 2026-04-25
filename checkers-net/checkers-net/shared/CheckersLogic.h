#pragma once
// =============================================================================
// CheckersLogic.h  —  Pure game rules, zero networking
// =============================================================================
// Board layout (8×8, row 0 = top, row 7 = bottom)
//   Cell values:  0=empty  1=P1  2=P2  3=P1King  4=P2King
//   Dark squares: (row+col) % 2 == 0  (pieces only ever live here)
//
// P1 starts rows 5-7 (bottom), moves UP   (dr = -1)
// P2 starts rows 0-2 (top),    moves DOWN (dr = +1)
// Kings can move both directions.
//
// Rules enforced:
//   • Mandatory capture  — if any jump available, must jump
//   • Multi-jump         — after a capture, must continue if further jump exists
//   • King promotion     — P1 reaching row 0, P2 reaching row 7
//   • Win condition      — opponent has no legal moves (no pieces OR blocked)
// =============================================================================

#include <array>
#include <vector>
#include <string>

using Board = std::array<std::array<int, 8>, 8>;

struct Move {
    int fromRow, fromCol;
    int toRow,   toCol;
    bool isCapture      = false;
    int  capturedRow    = -1;
    int  capturedCol    = -1;
};

struct GameState {
    Board board;
    int   currentPlayer = 1;   // 1 or 2
    bool  gameOver      = false;
    int   winner        = 0;   // 0=none, 1=P1, 2=P2

    // Multi-jump tracking: after a capture, must continue with same piece
    bool inMultiJump    = false;
    int  multiJumpRow   = -1;
    int  multiJumpCol   = -1;
};

class CheckersLogic {
public:
    // Build the starting board & state
    static GameState initialState();

    // All legal moves for the current player (mandatory-capture enforced)
    static std::vector<Move> validMoves(const GameState& state);

    // Apply move m to state (validates internally). Returns false if illegal.
    static bool applyMove(GameState& state, const Move& m);

    // Serialize board to a 64-char string (for network transport)
    static std::string boardToString(const Board& board);
    static Board        boardFromString(const std::string& s);

    // Pretty-print for debugging
    static void printBoard(const Board& board);

private:
    static bool isPlayerPiece  (int cell, int player);
    static bool isOpponentPiece(int cell, int player);
    static bool isKing         (int cell);
    static std::vector<int> directions(int cell);   // list of dr values

    static std::vector<Move> capturesFor(const Board& b, int player, int r, int c);
    static std::vector<Move> movesFor   (const Board& b, int player, int r, int c);
};

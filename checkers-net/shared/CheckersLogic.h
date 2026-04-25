#pragma once
// Pure game logic - no networking. Board is 8x8, stored as a flat array.
// Cell values: 0=empty, 1=P1, 2=P2, 3=P1 King, 4=P2 King
// P1 starts rows 5-7 (bottom), moves up. P2 starts rows 0-2 (top), moves down.
// Dark squares: (row+col) % 2 == 0

#include <array>
#include <vector>
#include <string>

using Board = std::array<std::array<int, 8>, 8>;

struct Move {
    int fromRow, fromCol;
    int toRow,   toCol;
    bool isCapture   = false;
    int  capturedRow = -1;
    int  capturedCol = -1;
};

struct GameState {
    Board board;
    int   currentPlayer = 1;
    bool  gameOver      = false;
    int   winner        = 0;
    // Tracks multi-jump: after a capture, the same piece must keep jumping
    bool  inMultiJump   = false;
    int   multiJumpRow  = -1;
    int   multiJumpCol  = -1;
};

class CheckersLogic {
public:
    static GameState         initialState();
    static std::vector<Move> validMoves(const GameState& state);
    static bool              applyMove(GameState& state, const Move& m);
    static std::string       boardToString(const Board& board);
    static Board             boardFromString(const std::string& s);
    static void              printBoard(const Board& board);

private:
    static bool isPlayerPiece  (int cell, int player);
    static bool isOpponentPiece(int cell, int player);
    static bool isKing         (int cell);
    static std::vector<int>  directions(int cell);
    static std::vector<Move> capturesFor(const Board& b, int player, int r, int c);
    static std::vector<Move> movesFor   (const Board& b, int player, int r, int c);
};

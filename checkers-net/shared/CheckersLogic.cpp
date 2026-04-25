#include "CheckersLogic.h"
#include <iostream>
#include <stdexcept>

bool CheckersLogic::isPlayerPiece(int cell, int player) {
    return player == 1 ? (cell == 1 || cell == 3) : (cell == 2 || cell == 4);
}

bool CheckersLogic::isOpponentPiece(int cell, int player) {
    return player == 1 ? (cell == 2 || cell == 4) : (cell == 1 || cell == 3);
}

bool CheckersLogic::isKing(int cell) {
    return cell == 3 || cell == 4;
}

// Returns valid row directions for a piece (kings move both ways)
std::vector<int> CheckersLogic::directions(int cell) {
    if (cell == 1)          return {-1};
    if (cell == 2)          return {+1};
    if (cell == 3 || cell == 4) return {-1, +1};
    return {};
}

GameState CheckersLogic::initialState() {
    GameState state;
    for (auto& row : state.board) row.fill(0);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 8; c++)
            if ((r + c) % 2 == 0) state.board[r][c] = 2;
    for (int r = 5; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if ((r + c) % 2 == 0) state.board[r][c] = 1;
    state.currentPlayer = 1;
    return state;
}

// Returns all capture moves available for a piece at (r,c)
std::vector<Move> CheckersLogic::capturesFor(const Board& b, int player, int r, int c) {
    std::vector<Move> caps;
    int cell = b[r][c];
    if (!isPlayerPiece(cell, player)) return caps;
    for (int dr : directions(cell)) {
        for (int dc : {-1, +1}) {
            int mr = r + dr,   mc = c + dc;
            int lr = r + 2*dr, lc = c + 2*dc;
            if (lr < 0 || lr > 7 || lc < 0 || lc > 7) continue;
            if (!isOpponentPiece(b[mr][mc], player)) continue;
            if (b[lr][lc] != 0) continue;
            caps.push_back({r, c, lr, lc, true, mr, mc});
        }
    }
    return caps;
}

// Returns all regular (non-capture) moves for a piece at (r,c)
std::vector<Move> CheckersLogic::movesFor(const Board& b, int player, int r, int c) {
    std::vector<Move> moves;
    int cell = b[r][c];
    if (!isPlayerPiece(cell, player)) return moves;
    for (int dr : directions(cell)) {
        for (int dc : {-1, +1}) {
            int nr = r + dr, nc = c + dc;
            if (nr < 0 || nr > 7 || nc < 0 || nc > 7) continue;
            if (b[nr][nc] != 0) continue;
            moves.push_back({r, c, nr, nc, false, -1, -1});
        }
    }
    return moves;
}

// Returns legal moves for current player. Captures are mandatory.
std::vector<Move> CheckersLogic::validMoves(const GameState& state) {
    if (state.gameOver) return {};
    if (state.inMultiJump)
        return capturesFor(state.board, state.currentPlayer,
                           state.multiJumpRow, state.multiJumpCol);

    std::vector<Move> caps, regular;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (!isPlayerPiece(state.board[r][c], state.currentPlayer)) continue;
            auto c2 = capturesFor(state.board, state.currentPlayer, r, c);
            auto m2 = movesFor  (state.board, state.currentPlayer, r, c);
            caps.insert(caps.end(), c2.begin(), c2.end());
            regular.insert(regular.end(), m2.begin(), m2.end());
        }
    }
    return caps.empty() ? regular : caps;
}

// Applies a move. Returns false if the move isn't in the legal set.
bool CheckersLogic::applyMove(GameState& state, const Move& m) {
    auto legal = validMoves(state);
    bool found = false;
    for (const auto& v : legal)
        if (v.fromRow==m.fromRow && v.fromCol==m.fromCol &&
            v.toRow==m.toRow && v.toCol==m.toCol) { found = true; break; }
    if (!found) return false;

    int piece = state.board[m.fromRow][m.fromCol];
    state.board[m.fromRow][m.fromCol] = 0;
    state.board[m.toRow  ][m.toCol  ] = piece;
    if (m.isCapture) state.board[m.capturedRow][m.capturedCol] = 0;

    // King promotion
    bool justKinged = false;
    if (piece == 1 && m.toRow == 0) { state.board[m.toRow][m.toCol] = 3; justKinged = true; }
    if (piece == 2 && m.toRow == 7) { state.board[m.toRow][m.toCol] = 4; justKinged = true; }

    // After a capture, check for further captures (multi-jump)
    if (m.isCapture && !justKinged) {
        auto further = capturesFor(state.board, state.currentPlayer, m.toRow, m.toCol);
        if (!further.empty()) {
            state.inMultiJump  = true;
            state.multiJumpRow = m.toRow;
            state.multiJumpCol = m.toCol;
            return true;
        }
    }

    state.inMultiJump   = false;
    state.currentPlayer = (state.currentPlayer == 1) ? 2 : 1;

    // Win check: if the new current player has no moves, the previous player wins
    if (validMoves(state).empty()) {
        state.gameOver = true;
        state.winner   = (state.currentPlayer == 1) ? 2 : 1;
    }
    return true;
}

std::string CheckersLogic::boardToString(const Board& board) {
    std::string s;
    s.reserve(64);
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            s += static_cast<char>('0' + board[r][c]);
    return s;
}

Board CheckersLogic::boardFromString(const std::string& s) {
    if (s.size() != 64) throw std::runtime_error("boardFromString: expected 64 chars");
    Board b;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            b[r][c] = s[r * 8 + c] - '0';
    return b;
}

void CheckersLogic::printBoard(const Board& board) {
    std::cout << "   0  1  2  3  4  5  6  7\n";
    for (int r = 0; r < 8; r++) {
        std::cout << r << " ";
        for (int c = 0; c < 8; c++) {
            int v = board[r][c];
            if      (v == 0) std::cout << "[ ]";
            else if (v == 1) std::cout << "[o]";
            else if (v == 2) std::cout << "[x]";
            else if (v == 3) std::cout << "[O]";
            else             std::cout << "[X]";
        }
        std::cout << "\n";
    }
}

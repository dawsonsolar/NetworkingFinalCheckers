#include "CheckersLogic.h"
#include <iostream>
#include <stdexcept>

// ── Helpers ────────────────────────────────────────────────────────────────

bool CheckersLogic::isPlayerPiece(int cell, int player) {
    if (player == 1) return cell == 1 || cell == 3;
    if (player == 2) return cell == 2 || cell == 4;
    return false;
}

bool CheckersLogic::isOpponentPiece(int cell, int player) {
    if (player == 1) return cell == 2 || cell == 4;
    if (player == 2) return cell == 1 || cell == 3;
    return false;
}

bool CheckersLogic::isKing(int cell) {
    return cell == 3 || cell == 4;
}

// Returns the row-direction(s) a piece can move
std::vector<int> CheckersLogic::directions(int cell) {
    if (cell == 1)          return {-1};       // P1 moves up
    if (cell == 2)          return {+1};       // P2 moves down
    if (cell == 3 || cell == 4) return {-1, +1}; // Kings both ways
    return {};
}

// ── Initial state ──────────────────────────────────────────────────────────

GameState CheckersLogic::initialState() {
    GameState state;
    for (auto& row : state.board) row.fill(0);

    // P2 on rows 0–2, dark squares: (r+c)%2==0
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 8; c++)
            if ((r + c) % 2 == 0)
                state.board[r][c] = 2;

    // P1 on rows 5–7, dark squares
    for (int r = 5; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if ((r + c) % 2 == 0)
                state.board[r][c] = 1;

    state.currentPlayer = 1;
    state.gameOver      = false;
    state.winner        = 0;
    state.inMultiJump   = false;
    return state;
}

// ── Move generation ────────────────────────────────────────────────────────

// All capture moves available for piece at (r,c)
std::vector<Move> CheckersLogic::capturesFor(const Board& b, int player, int r, int c) {
    std::vector<Move> caps;
    int cell = b[r][c];
    if (!isPlayerPiece(cell, player)) return caps;

    for (int dr : directions(cell)) {
        for (int dc : {-1, +1}) {
            int mr = r + dr,    mc = c + dc;   // middle (captured) square
            int lr = r + 2*dr,  lc = c + 2*dc; // landing square
            if (lr < 0 || lr > 7 || lc < 0 || lc > 7) continue;
            if (!isOpponentPiece(b[mr][mc], player)) continue;
            if (b[lr][lc] != 0) continue;
            Move m;
            m.fromRow = r; m.fromCol = c;
            m.toRow   = lr; m.toCol  = lc;
            m.isCapture   = true;
            m.capturedRow = mr; m.capturedCol = mc;
            caps.push_back(m);
        }
    }
    return caps;
}

// All non-capture moves available for piece at (r,c)
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

// All legal moves for the current player (mandatory capture enforced)
std::vector<Move> CheckersLogic::validMoves(const GameState& state) {
    if (state.gameOver) return {};

    // Mid-multi-jump: only further captures from the jumping piece
    if (state.inMultiJump) {
        return capturesFor(state.board, state.currentPlayer,
                           state.multiJumpRow, state.multiJumpCol);
    }

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
    // Mandatory capture rule
    return caps.empty() ? regular : caps;
}

// ── Apply move ─────────────────────────────────────────────────────────────

bool CheckersLogic::applyMove(GameState& state, const Move& m) {
    // Validate that this move is in the legal set
    auto legal = validMoves(state);
    bool found = false;
    for (const auto& v : legal) {
        if (v.fromRow == m.fromRow && v.fromCol == m.fromCol &&
            v.toRow   == m.toRow   && v.toCol   == m.toCol) {
            found = true;
            break;
        }
    }
    if (!found) return false;

    // Move the piece
    int piece = state.board[m.fromRow][m.fromCol];
    state.board[m.fromRow][m.fromCol] = 0;
    state.board[m.toRow  ][m.toCol  ] = piece;

    // Remove captured piece
    if (m.isCapture) {
        state.board[m.capturedRow][m.capturedCol] = 0;
    }

    // King promotion (piece must not already be a king)
    bool justKinged = false;
    if (piece == 1 && m.toRow == 0) {
        state.board[m.toRow][m.toCol] = 3;
        justKinged = true;
    } else if (piece == 2 && m.toRow == 7) {
        state.board[m.toRow][m.toCol] = 4;
        justKinged = true;
    }

    // Multi-jump: after a capture, check for further captures from landing square
    // (no multi-jump on the turn a piece is kinged — standard rule)
    if (m.isCapture && !justKinged) {
        auto further = capturesFor(state.board, state.currentPlayer, m.toRow, m.toCol);
        if (!further.empty()) {
            state.inMultiJump  = true;
            state.multiJumpRow = m.toRow;
            state.multiJumpCol = m.toCol;
            return true; // Same player's turn continues
        }
    }

    // End of this player's turn — switch players
    state.inMultiJump   = false;
    state.multiJumpRow  = -1;
    state.multiJumpCol  = -1;
    state.currentPlayer = (state.currentPlayer == 1) ? 2 : 1;

    // Win check: if the new current player has no legal moves, previous player wins
    auto nextMoves = validMoves(state);
    if (nextMoves.empty()) {
        state.gameOver = true;
        state.winner   = (state.currentPlayer == 1) ? 2 : 1;
    }

    return true;
}

// ── Serialization ──────────────────────────────────────────────────────────

std::string CheckersLogic::boardToString(const Board& board) {
    std::string s;
    s.reserve(64);
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            s += static_cast<char>('0' + board[r][c]);
    return s;
}

Board CheckersLogic::boardFromString(const std::string& s) {
    if (s.size() != 64)
        throw std::runtime_error("boardFromString: expected 64 chars");
    Board b;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            b[r][c] = s[r * 8 + c] - '0';
    return b;
}

// ── Debug ──────────────────────────────────────────────────────────────────

void CheckersLogic::printBoard(const Board& board) {
    std::cout << "   0  1  2  3  4  5  6  7\n";
    for (int r = 0; r < 8; r++) {
        std::cout << r << " ";
        for (int c = 0; c < 8; c++) {
            int v = board[r][c];
            const char* sym = "[ ]";
            if (v == 1) sym = "[o]";
            else if (v == 2) sym = "[x]";
            else if (v == 3) sym = "[O]";
            else if (v == 4) sym = "[X]";
            std::cout << sym;
        }
        std::cout << "\n";
    }
}

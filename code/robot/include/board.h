#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

// Column-major bitboard: a cell is bit (7 * col + row), row 0 at the bottom.
// Each column gets 7 bits for 6 cells; the spare row-6 bit is a sentinel that
// is never filled, so a four-in-a-row shift can never wrap into the next
// column. That is what lets containsWin be four shift-and pairs.
#define COLUMN_MASK 0x3FULL
#define BOARD_MASK 0x0000FDFBF7EFDFBFULL


struct Board {
    // Pieces are stored by turn order, not display colour. This keeps search
    // and memoization independent of whether the first player is red/yellow
    // or controlled by the robot/human.
    uint64_t firstPieces;
    uint64_t allPieces;
};


inline uint64_t getSecondPieces(const Board& board) {
    return board.firstPieces ^ board.allPieces;
}

inline uint64_t getRedPieces(const Board& board, bool firstIsRed) {
    return firstIsRed ? board.firstPieces : getSecondPieces(board);
}

inline uint64_t getYellowPieces(const Board& board, bool firstIsRed) {
    return firstIsRed ? getSecondPieces(board) : board.firstPieces;
}

inline bool isPieceRowCol(uint64_t pieces, uint8_t row, uint8_t col) {
    uint64_t mask = 1ULL << (7 * col + row);
    return (pieces & mask) != 0;
}

inline bool isPiecePos(uint64_t pieces, uint8_t pos) {
    uint64_t mask = 1ULL << pos;
    return (pieces & mask) != 0;
}

inline bool isPieceMask(uint64_t pieces, uint64_t mask) {
    return (pieces & mask) != 0;
}

inline bool isColumnFull(const Board& board, uint8_t col) {
    uint64_t mask = 1ULL << (7 * col + 5);
    return (board.allPieces & mask) != 0;
}

inline uint8_t getColumnHeight(const Board& board, uint8_t col) {
    uint64_t mask = COLUMN_MASK << (7 * col);
    return static_cast<uint8_t>(__builtin_popcountll(board.allPieces & mask));
}

inline bool isBoardFull(const Board& board) {
    return (board.allPieces & BOARD_MASK) == BOARD_MASK;
}

bool containsWin(uint64_t pieces);
void placeFirstPiece(Board& board, uint8_t col);
void placeSecondPiece(Board& board, uint8_t col);

// A collision-free rank of every gravity-valid board with alternating piece
// counts, reversibly mixed across 47 bits for an even memo-table distribution.
// The first player is implied by firstPieces; display colour is not part of it.
uint64_t getBoardKey(const Board& board);

#endif // BOARD_H

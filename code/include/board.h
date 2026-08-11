#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>


struct Board {
    uint64_t redPieces;
    uint64_t allPieces;
};


inline uint64_t getYellowPieces(const Board& board) {
    return board.redPieces ^ board.allPieces;
}

inline bool isPieceRowCol(uint64_t pieces, uint8_t row, uint8_t col) {
    uint8_t bitPos = 7 * row + col;
    uint64_t mask = 1 << bitPos;
    return (pieces & mask) != 0;
}

inline bool isPiecePos(uint64_t pieces, uint8_t pos) {
    uint64_t mask = 1 << pos;
    return (pieces & mask) != 0;
}

inline bool isPieceMask(uint64_t pieces, uint64_t mask) {
    return (pieces & mask) != 0;
}

inline bool canPlacePiece(const Board& board, uint8_t col) {
    uint64_t mask = 1 << (col + 35);
    return (board.allPieces & mask) == 0;
}

bool containsWin(uint64_t pieces);
void placeRedPiece(Board& board, uint8_t col);
void placeYellowPiece(Board& board, uint8_t col);
uint32_t hashBoard(const Board& board);

#endif // BOARD_H

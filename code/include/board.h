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

inline bool isPieceRowCol(uint64_t pieces, int row, int col) {
    int bitPos = 7 * row + col;
    uint64_t mask = (1 << bitPos);
    return (pieces & mask) != 0;
}

inline bool isPiecePos(uint64_t pieces, int pos) {
    uint64_t mask = (1 << pos);
    return (pieces & mask) != 0;
}

inline bool isPieceMask(uint64_t pieces, uint64_t mask) {
    return (pieces & mask) != 0;
}

bool containsWin(uint64_t pieces);


#endif // BOARD_H

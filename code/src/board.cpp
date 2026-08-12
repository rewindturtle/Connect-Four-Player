#include "board.h"


bool containsWin(uint64_t pieces) {
    // Step 1 is vertical, 7 horizontal, 8 the "/" diagonal, 6 the "\" diagonal.
    // Pairing a step with double that step tests all four cells at once.
    uint64_t m = pieces & (pieces >> 1);
    if (m & (m >> 2)) return true;

    m = pieces & (pieces >> 7);
    if (m & (m >> 14)) return true;

    m = pieces & (pieces >> 8);
    if (m & (m >> 16)) return true;

    m = pieces & (pieces >> 6);
    return (m & (m >> 12)) != 0;
}


// Adding the column's bottom bit to its occupied cells carries up into the
// lowest empty one, which is where the piece lands.
static uint64_t landingMask(const Board& board, uint8_t col) {
    uint8_t idx = 7 * col;
    uint64_t colMask = COLUMN_MASK << idx;
    return ((board.allPieces & colMask) + (1ULL << idx)) & colMask;
}


void placeRedPiece(Board& board, uint8_t col) {
    uint64_t mask = landingMask(board, col);
    board.redPieces |= mask;
    board.allPieces |= mask;
}


void placeYellowPiece(Board& board, uint8_t col) {
    board.allPieces |= landingMask(board, col);
}


inline static uint64_t murmur3_64(uint64_t key) {
	key ^= key >> 33;
	key *= 0xff51afd7ed558ccd;
	key ^= key >> 33;
	key *= 0xc4ceb9fe1a85ec53;
	key ^= key >> 33;
	return key;
}


uint64_t hashBoard(const Board& board) {
    uint64_t hash = board.allPieces;
    hash ^= murmur3_64(board.redPieces);
    return murmur3_64(hash);
}

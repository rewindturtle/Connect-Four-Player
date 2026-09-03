#include "board.h"

#include <assert.h>


Board::Board(uint64_t firstPieces, uint64_t allPieces)
    : _firstPieces(firstPieces), _allPieces(allPieces) {}


bool Board::containsWin(uint64_t pieces) {
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
uint64_t Board::_landingMask(uint8_t col) const {
    uint8_t idx = 7 * col;
    uint64_t colMask = COLUMN_MASK << idx;
    return ((_allPieces & colMask) + (1ULL << idx)) & colMask;
}


void Board::placeFirstPiece(uint8_t col) {
    uint64_t mask = _landingMask(col);
    _firstPieces |= mask;
    _allPieces |= mask;
}


void Board::placeSecondPiece(uint8_t col) {
    _allPieces |= _landingMask(col);
}


uint64_t Board::getKey() const {
    assert((_firstPieces & ~_allPieces) == 0);

    // _allPieces + BOTTOM_MASK carries each column's run of occupied bits into
    // a single height marker. The bits below that marker then record which
    // occupied cells belong to the first player. These fields are disjoint,
    // making the 49-bit encoding exact for every gravity-valid board.
    uint64_t key = _firstPieces | (_allPieces + BOTTOM_MASK);

    // Each operation is reversible modulo 2^49. The right-xors bring the
    // upper columns into the low table-index bits; multiplication by an odd
    // constant preserves uniqueness while spreading nearby positions.
    key ^= key >> 25;
    key = (key * 0x9E3779B97F4A7C15ULL) & BOARD_KEY_MASK;
    key ^= key >> 23;
    return key & BOARD_KEY_MASK;
}

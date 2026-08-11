#include "board.h"


bool doesRowContainWin(uint64_t pieces, uint8_t row) {
    // Check middle position first.
    // If middle pos is not filled, there is no win in this row
    uint64_t mask = 1 << (row + 3);
    if (!isPieceMask(pieces, mask)) {
        // X X X 0 X X X
        return false;
    }

    // X X X 1 X X X
    // Check the middle positions neighbours
    mask = 1 << (row + 2);
    if (isPieceMask(pieces, mask)) {

        // X X 1 1 X X X
        mask = 1 << (row + 4);
        if (isPieceMask(pieces, mask)) {
            // X X 1 1 1 X X
            mask = 1 << (row + 5);
            if (isPieceMask(pieces, mask)) {
                // Columns 2, 3, 4, 5
                // X X 1 1 1 1 X
                return true;
            }

            mask = 1 << (row + 1);
            // If true:  X 1 1 1 1 0 X
            // If false: X 0 1 1 1 0 X
            return isPieceMask(pieces, mask);
        }
        
        // X X 1 1 0 X X
        mask = 1 << (row + 1);
        if (!isPieceMask(pieces, mask)) {
            // X 0 1 1 0 X X
            return false;
        }

        // If true:  1 1 1 1 0 X X
        // If false: 0 1 1 1 0 X X
        mask = 1 << row;
        return isPieceMask(pieces, mask);
    }

    // X X 0 1 X X X
    mask = 1 << (row + 4);
    if (!isPieceMask(pieces, mask)) {
        // X X 0 1 0 X X
        return false;
    }

    // X X 0 1 1 X X
    mask = 1 << (row + 5);
    if (!isPieceMask(pieces, mask)) {
        // X X 0 1 1 0 X
        return false;
    }

    mask = 1 << (row + 6);
    // If true:  X X 0 1 1 1 1
    // If false: X X 0 1 1 1 0
    return isPieceMask(pieces, mask);
}


bool doesColContainWin(uint64_t pieces, uint8_t col) {
    // If rows 2 and 3 do not contain pieces, there is no win

    // Row Indices
    // 0: 0
    // 1: 7
    // 2: 14
    // 3: 21
    // 4: 28
    // 5: 35

    uint64_t mask = 1 << (col + 14);
    if (!isPieceMask(pieces, mask)) {
        // X X 0 X X X
        return false;
    }

    mask = 1 << (col + 21);
    if (!isPieceMask(pieces, mask)) {
        // X X 1 0 X X
        return false;
    }

    // X X 1 1 X X
    mask = 1 << (col + 7);
    if (isPieceMask(pieces, mask)) {
        // X 1 1 1 X X
        mask = 1 << col;
        if (isPieceMask(pieces, mask)) {
            // 1 1 1 1 X X
            return true;
        }

        mask = 1 << (col + 28);
        // If true:  0 1 1 1 1 X
        // If false: 0 1 1 1 0 X
        return isPieceMask(pieces, mask);
    }

    // X 0 1 1 X X
    mask = 1 << (col + 28);
    if (!isPieceMask(pieces, mask)) {
        // X 0 1 1 0 X
        return false;
    }

    mask = 1 << (col + 35);
    // If true:  X 0 1 1 1 1
    // If false: X 0 1 1 1 0
    return isPieceMask(pieces, mask);
}


bool containsWin(uint64_t pieces) {
    // Check for row wins
    for (uint8_t r = 0; r < 42; r += 7) {
        if (doesRowContainWin(pieces, r)) return true;
    }

    // Check for column wins
    for (uint8_t c = 0; c < 7; ++c) {
        if (doesColContainWin(pieces, c)) return true;
    }

    return false;
}

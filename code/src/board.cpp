#include "board.h"


static uint32_t ZOBRIST_TABLE[84] = {
    967904677, 681638290, 1002504380, 3861667980, 1604002761, 4009905169, 1753263609,
    2522237384, 3680866771, 3191444279, 2812598900, 281029340, 594367001, 2195930867,
    3529159130, 3018220923, 58848470, 2470600206, 1059663100, 2449176635, 3654968481,
    2009224734, 1150442302, 1234336178, 933293516, 646462091, 3071600039, 3738331963,
    223131513, 1583910619, 2433463551, 228925100, 3213140429, 935429426, 3028084537,
    3211422253, 1464864881, 503138217, 1682321086, 610802923, 3761524265, 2489172908,
    2851065787, 3197227328, 1056695448, 65386577, 830449308, 114513832, 4147497718,
    2284396671, 3874989439, 3640508808, 3494048737, 2905549677, 2558299500, 3018429405,
    3133170435, 1798210310, 425863577, 2900838906, 2605561693, 1674351717, 716997829,
    1022367825, 796286050, 2167781931, 4097526168, 1534508911, 1851287387, 1760662667,
    671804348, 1820482289, 1290561639, 3448761715, 1525370725, 1411345039, 1027353773,
    1963917863, 362287306, 2975220266, 1000536181, 1260763529, 836355917, 2375511535
};


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


bool doesRightDiagContainWin(uint64_t pieces) {
    for (uint8_t r = 0; r < 28; r += 7) {
        for (uint8_t c = 0; c < 4; ++c) {
            uint8_t bitPos = r + c;
            if (!isPiecePos(pieces, bitPos)) {
                continue;
            }

            bitPos += 8;
            if (!isPiecePos(pieces, bitPos)) {
                continue;
            }

            bitPos += 8;
            if (!isPiecePos(pieces, bitPos)) {
                continue;
            }

            bitPos += 8;
            if (isPiecePos(pieces, bitPos)) {
                return true;
            }
        }
    }

    return false;
}


bool doesLeftDiagContainWin(uint64_t pieces) {
    for (uint8_t r = 0; r < 28; r += 7) {
        for (uint8_t c = 0; c < 4; ++c) {
            uint8_t bitPos = r + c + 24; // 7 * 3 + 3
            if (!isPiecePos(pieces, bitPos)) {
                continue;
            }

            bitPos -= 8;
            if (!isPiecePos(pieces, bitPos)) {
                continue;
            }

            bitPos -= 8;
            if (!isPiecePos(pieces, bitPos)) {
                continue;
            }

            bitPos -= 8;
            if (isPiecePos(pieces, bitPos)) {
                return true;
            }
        }
    }

    return false;
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

    if (doesRightDiagContainWin(pieces)) return true;
    return doesLeftDiagContainWin(pieces);
}


void placeRedPiece(Board& board, uint8_t col) {
    for (uint8_t p = col; p < 42; p += 7) {
        uint64_t mask = 1 << p;
        if (!isPieceMask(board.allPieces, mask)) {
            board.redPieces |= mask;
            board.allPieces |= mask;
            return;
        }
    }
}


void placeYellowPiece(Board& board, uint8_t col) {
    for (uint8_t p = col; p < 42; p += 7) {
        uint64_t mask = 1 << p;
        if (!isPieceMask(board.allPieces, mask)) {
            board.allPieces |= mask;
            return;
        }
    }
}


uint32_t hashBoard(const Board& board) {
    uint32_t hash = 0;

    for (uint8_t i = 0; i < 42; ++i) {
        uint64_t mask = 1 << i;
        if ((board.allPieces & mask) == 0) continue;

        if ((board.redPieces & mask) != 0) {
            hash ^= ZOBRIST_TABLE[i];
        } else {
            hash ^= ZOBRIST_TABLE[i + 42];
        }
    }

    return hash;
}

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


uint32_t hashBoard(const Board& board) {
    static const uint32_t ZOBRIST_TABLE[84] = {
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

    uint32_t hash = 0;
    uint8_t cell = 0;
    for (uint8_t col = 0; col < 49; col += 7) {
        for (uint8_t row = 0; row < 6; ++row) {
            uint64_t mask = 1ULL << (col + row);
            if ((board.allPieces & mask) == 0) {
                ++cell;
                continue;
            }

            if ((board.redPieces & mask) != 0) {
                hash ^= ZOBRIST_TABLE[cell];
            } else {
                hash ^= ZOBRIST_TABLE[cell + 42];
            }

            ++cell;
        }
    }

    return hash;
}

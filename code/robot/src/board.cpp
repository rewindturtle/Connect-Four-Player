#include "board.h"

#include <assert.h>


constexpr uint8_t NUM_COLUMNS = 7;
constexpr uint8_t COLUMN_HEIGHT = 6;
constexpr uint8_t MAX_PIECES = NUM_COLUMNS * COLUMN_HEIGHT;
constexpr uint8_t MAX_FIRST_PIECES = (MAX_PIECES + 1) / 2;
constexpr uint64_t BOARD_KEY_MASK = (1ULL << 47) - 1;

namespace {
    struct RankTables {
        uint64_t combinations[MAX_PIECES + 1][MAX_FIRST_PIECES + 1] = {};
        uint32_t heightWays[NUM_COLUMNS + 1][MAX_PIECES + 1] = {};
        uint64_t turnOffsets[MAX_PIECES + 2] = {};
    };


static constexpr RankTables makeRankTables() {
    RankTables tables;

    tables.combinations[0][0] = 1;
    for (uint8_t n = 1; n <= MAX_PIECES; ++n) {
        tables.combinations[n][0] = 1;
        uint8_t maxSelected = n < MAX_FIRST_PIECES ? n : MAX_FIRST_PIECES;
        for (uint8_t selected = 1; selected <= maxSelected; ++selected) {
            tables.combinations[n][selected] = tables.combinations[n - 1][selected - 1] + tables.combinations[n - 1][selected];
        }
    }

    tables.heightWays[0][0] = 1;
    for (uint8_t columns = 1; columns <= NUM_COLUMNS; ++columns) {
        for (uint8_t pieces = 0; pieces <= MAX_PIECES; ++pieces) {
            for (uint8_t height = 0; height <= COLUMN_HEIGHT && height <= pieces; ++height) {
                tables.heightWays[columns][pieces] += tables.heightWays[columns - 1][pieces - height];
            }
        }
    }

    for (uint8_t pieces = 0; pieces <= MAX_PIECES; ++pieces) {
        uint8_t firstPieces = (pieces + 1) / 2;
        tables.turnOffsets[pieces + 1] = tables.turnOffsets[pieces]
            + static_cast<uint64_t>(tables.heightWays[NUM_COLUMNS][pieces])
                * tables.combinations[pieces][firstPieces];
    }

    return tables;
}


static constexpr RankTables RANK_TABLES = makeRankTables();
static constexpr uint64_t NUM_RANKED_BOARDS = 70728639995483ULL;

static_assert(RANK_TABLES.turnOffsets[MAX_PIECES + 1] == NUM_RANKED_BOARDS,
              "board-rank tables must cover the expected state space");
static_assert(NUM_RANKED_BOARDS > (1ULL << 46) && NUM_RANKED_BOARDS < (1ULL << 47),
              "alternating gravity-valid boards must require exactly 47 bits");


static uint64_t rankHeightProfile(const Board& board, uint8_t pieces) {
    uint64_t rank = 0;
    uint8_t remainingPieces = pieces;

    for (uint8_t col = 0; col < NUM_COLUMNS; ++col) {
        uint64_t column = (board.allPieces >> (7 * col)) & COLUMN_MASK;
        uint8_t height = static_cast<uint8_t>(__builtin_popcountll(column));
        assert(column == ((1ULL << height) - 1));

        uint8_t remainingColumns = NUM_COLUMNS - col - 1;
        for (uint8_t candidate = 0; candidate < height; ++candidate) {
            rank += RANK_TABLES.heightWays[remainingColumns][remainingPieces - candidate];
        }

        remainingPieces -= height;
    }

    assert(remainingPieces == 0);
    return rank;
}


static uint64_t rankFirstPieceLocations(const Board& board, uint8_t expectedFirstPieces) {
    (void)expectedFirstPieces;

    uint64_t rank = 0;
    uint8_t occupiedIndex = 0;
    uint8_t selected = 0;

    for (uint8_t col = 0; col < NUM_COLUMNS; ++col) {
        uint64_t column = (board.allPieces >> (7 * col)) & COLUMN_MASK;
        uint8_t height = static_cast<uint8_t>(__builtin_popcountll(column));

        for (uint8_t row = 0; row < height; ++row) {
            if (isPieceRowCol(board.firstPieces, row, col)) {
                ++selected;
                rank += RANK_TABLES.combinations[occupiedIndex][selected];
            }

            ++occupiedIndex;
        }
    }

    assert(selected == expectedFirstPieces);
    return rank;
}


// Every operation is a permutation modulo 2^47: right-xors are reversible,
// and multiplication by an odd number is reversible. Splitting this result
// into a table slot and tag therefore cannot introduce aliases.
static uint64_t permuteBoardRank(uint64_t rank) {
    rank ^= rank >> 30;
    rank = (0xBF58476D1CE4E5B9ULL * rank) & BOARD_KEY_MASK;
    rank ^= rank >> 27;
    rank = (0x94D049BB133111EBULL * rank) & BOARD_KEY_MASK;
    rank ^= rank >> 31;
    return rank & BOARD_KEY_MASK;
}

} // namespace


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


void placeFirstPiece(Board& board, uint8_t col) {
    uint64_t mask = landingMask(board, col);
    board.firstPieces |= mask;
    board.allPieces |= mask;
}


void placeSecondPiece(Board& board, uint8_t col) {
    board.allPieces |= landingMask(board, col);
}


uint64_t getBoardKey(const Board& board) {
    assert((board.firstPieces & ~board.allPieces) == 0);

    uint8_t pieces = static_cast<uint8_t>(__builtin_popcountll(board.allPieces));
    uint8_t firstPieces = static_cast<uint8_t>(__builtin_popcountll(board.firstPieces));
    uint8_t expectedFirstPieces = (pieces + 1) / 2;

    (void)firstPieces;
    assert(firstPieces == expectedFirstPieces);

    uint64_t heightRank = rankHeightProfile(board, pieces);
    uint64_t colourRank = rankFirstPieceLocations(board, expectedFirstPieces);
    uint64_t colourWays = RANK_TABLES.combinations[pieces][expectedFirstPieces];
    uint64_t rank = RANK_TABLES.turnOffsets[pieces] + heightRank * colourWays + colourRank;

    assert(rank < RANK_TABLES.turnOffsets[pieces + 1]);
    return permuteBoardRank(rank);
}

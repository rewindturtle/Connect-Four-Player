#include "board.h"
#include "memo.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <unordered_set>
#include <vector>


static constexpr uint8_t MAX_TEST_PIECES = 9;
static constexpr uint64_t EXPECTED_TEST_KEYS = 897205;


static void addColourings(const std::vector<uint8_t>& cells, uint8_t index,
                          uint8_t remainingFirst, Board board,
                          std::unordered_set<uint64_t>& keys) {
    uint8_t remainingCells = static_cast<uint8_t>(cells.size() - index);
    if (remainingFirst > remainingCells) return;

    if (index == cells.size()) {
        assert(remainingFirst == 0);
        uint64_t key = board.getKey();
        assert(key <= BOARD_KEY_MASK);
        assert(keys.insert(key).second);
        return;
    }

    addColourings(cells, index + 1, remainingFirst, board, keys);

    if (remainingFirst != 0) {
        Board next(board.getFirstPieces() | (1ULL << cells[index]), board.getAllPieces());
        addColourings(cells, index + 1, remainingFirst - 1, next, keys);
    }
}


static void addHeightProfiles(uint8_t col, uint8_t remainingPieces, Board board,
                              std::unordered_set<uint64_t>& keys) {
    if (col == 7) {
        if (remainingPieces != 0) return;

        std::vector<uint8_t> cells;
        for (uint8_t c = 0; c < 7; ++c) {
            uint8_t height = board.getColumnHeight(c);
            for (uint8_t row = 0; row < height; ++row) {
                cells.push_back(7 * c + row);
            }
        }

        addColourings(cells, 0, static_cast<uint8_t>((cells.size() + 1) / 2), board, keys);
        return;
    }

    uint8_t maxHeight = remainingPieces < 6 ? remainingPieces : 6;
    for (uint8_t height = 0; height <= maxHeight; ++height) {
        uint64_t column = height == 0 ? 0 : (1ULL << height) - 1;
        Board next(board.getFirstPieces(), board.getAllPieces() | (column << (7 * col)));
        addHeightProfiles(col + 1, remainingPieces - height, next, keys);
    }
}


static void testBoardKeys() {
    std::unordered_set<uint64_t> keys;
    keys.reserve(EXPECTED_TEST_KEYS);

    for (uint8_t pieces = 0; pieces <= MAX_TEST_PIECES; ++pieces) {
        addHeightProfiles(0, pieces, Board{0, 0}, keys);
    }

    assert(keys.size() == EXPECTED_TEST_KEYS);

    // A slot collision is harmless because the rest of the exact key is kept.
    std::vector<uint64_t> keysBySlot(MEMO_SIZE, UINT64_MAX);
    bool foundSlotCollision = false;
    for (uint64_t key : keys) {
        uint32_t slot = key & MEMO_SLOT_MASK;
        if (keysBySlot[slot] != UINT64_MAX) {
            assert(keysBySlot[slot] != key);
            assert((keysBySlot[slot] >> MEMO_BITS) != (key >> MEMO_BITS));
            foundSlotCollision = true;
            break;
        }
        keysBySlot[slot] = key;
    }
    assert(foundSlotCollision);
}


static void testScoreEncoding() {
    for (uint8_t occupied = 0; occupied <= 42; ++occupied) {
        assert(Memo::encodeScore(0, occupied) == 0);
        assert(Memo::decodeScore(0, occupied) == 0);

        for (uint8_t distance = 0; occupied + distance <= 42; ++distance) {
            int8_t magnitude = MAX_SCORE - (occupied + distance);
            int8_t score = (distance & 1) != 0 ? magnitude : -magnitude;
            uint8_t code = Memo::encodeScore(score, occupied);

            assert(code == distance + 1);
            assert(code < 64);
            assert(Memo::decodeScore(code, occupied) == score);
        }
    }
}


static void testColourMapping() {
    Board board = {0, 0};
    board.placeFirstPiece(0);
    board.placeSecondPiece(1);

    assert(board.getRedPieces(true) == board.getFirstPieces());
    assert(board.getYellowPieces(true) == board.getSecondPieces());
    assert(board.getRedPieces(false) == board.getSecondPieces());
    assert(board.getYellowPieces(false) == board.getFirstPieces());
}


static void testMemoStorage() {
    static_assert(sizeof(MemoEntry) == 6);
    static_assert(MEMO_SIZE * sizeof(MemoEntry) == 12 * 1024 * 1024);

    Memo memo;
    assert(memo.isValid());
    assert(reinterpret_cast<uintptr_t>(memo.data()) % alignof(MemoEntry) == 0);

    constexpr uint32_t slot = 17;
    // Exercises the two tag bits that did not exist in the old 47-bit layout.
    constexpr uint32_t tag = 0x0EABCDEF;
    constexpr uint8_t occupied = 6;
    constexpr int8_t score = 120; // Win on the next move, at terminal ply 7.

    assert(memo.insertEntry(slot, tag, score, occupied, 8, MIN_SCORE, MAX_SCORE));
    assert(memo.isSlotInitialized(slot));
    assert(memo.getTag(slot) == tag);
    assert(memo.getScore(slot, occupied) == score);
    assert(memo.getDepth(slot) == 8);
    assert(memo.getFlag(slot) == MEMO_FLAG_EXACT);
    assert((memo.entry(slot).tagHigh & 0xF000) == 0);
    assert((memo.entry(slot).metadata & 0xC000) == 0);

    int8_t cachedScore = 0;
    assert(memo.probe(slot, tag, occupied, 8, MIN_SCORE, MAX_SCORE, cachedScore));
    assert(cachedScore == score);
    assert(!memo.probe(slot, tag ^ 1, occupied, 8, MIN_SCORE, MAX_SCORE, cachedScore));

    memo.reset();
    assert(!memo.isSlotInitialized(slot));
}


int main() {
    testBoardKeys();
    testScoreEncoding();
    testColourMapping();
    testMemoStorage();
    std::puts("memo tests passed");
    return 0;
}

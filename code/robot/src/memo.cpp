#include "memo.h"
#include "platform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


static constexpr size_t MEMO_BYTES = sizeof(MemoEntry) * MEMO_SIZE;


Memo::Memo()
    : _entries(static_cast<MemoEntry*>(c4Allocate(MEMO_BYTES))) {
    reset();
}


Memo::~Memo() {
    free(static_cast<void*>(_entries));
}


void Memo::reset() {
    if (_entries == nullptr) return;

    memset(_entries, 0, MEMO_BYTES);
}


bool Memo::probe(uint32_t slot, uint32_t tag, uint8_t occupied, uint8_t depth,
                 int8_t alpha, int8_t beta, int8_t& score) const {
    const MemoEntry& memoEntry = _entries[slot];
    uint16_t metadata = memoEntry.metadata;
    uint8_t storedDepth = entryDepth(metadata);
    if (storedDepth == 0 || entryTag(memoEntry) != tag) return false;

    score = decodeScore(entryScoreCode(metadata), occupied);

    // A non-zero score is a proven win or loss, which no deeper search can change.
    if (storedDepth < depth && score == 0) return false;

    switch (entryFlag(metadata)) {
        case MEMO_FLAG_EXACT:
            return true;
        case MEMO_FLAG_LB:
            return score >= beta;
        case MEMO_FLAG_UB:
            return score <= alpha;
        default:
            return false;
    }
}


uint8_t Memo::encodeScore(int8_t score, uint8_t occupied) {
    if (score == 0) return 0;

    uint8_t magnitude = static_cast<uint8_t>(score < 0 ? -static_cast<int16_t>(score) : score);
    uint8_t terminalPly = MAX_SCORE - magnitude;
    assert(terminalPly >= occupied && terminalPly <= 42);

    uint8_t distance = terminalPly - occupied;
    assert((score > 0) == ((distance & 1) != 0));
    assert(distance + 1 < 64);
    return distance + 1;
}


int8_t Memo::decodeScore(uint8_t scoreCode, uint8_t occupied) {
    if (scoreCode == 0) return 0;

    uint8_t distance = scoreCode - 1;
    int8_t magnitude = MAX_SCORE - (occupied + distance);
    return (distance & 1) != 0 ? magnitude : -magnitude;
}


int8_t Memo::getScore(uint32_t slot, uint8_t occupied) const {
    return decodeScore(getScoreCode(slot), occupied);
}


bool Memo::insertEntry(uint32_t slot, uint32_t tag, int8_t score,
                       uint8_t occupied, uint8_t depth, int8_t alpha, int8_t beta) {
    // More lookahead wins the slot. An empty slot has a depth of 0 and always
    // loses. A loaded book entry always loses to a real search.
    MemoEntry& memoEntry = _entries[slot];
    uint8_t storedDepth = entryDepth(memoEntry.metadata);
    if (storedDepth != BOOK_DEPTH && depth < storedDepth) return false;

    assert((tag & ~MEMO_TAG_MASK) == 0);
    uint8_t flag;
    if (score <= alpha) {
        flag = MEMO_FLAG_UB;
    } else if (score >= beta) {
        flag = MEMO_FLAG_LB;
    } else {
        flag = MEMO_FLAG_EXACT;
    }

    uint8_t scoreCode = encodeScore(score, occupied);
    memoEntry.tagLow = static_cast<uint16_t>(tag);
    memoEntry.tagHigh = static_cast<uint16_t>(tag >> 16);
    memoEntry.metadata = (static_cast<uint16_t>(scoreCode) << MEMO_SCORE_SHIFT) | flag | depth;
    return true;
}

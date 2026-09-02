#include "memo.h"
#include "platform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


static constexpr size_t TAG_AND_SCORE_BYTES = sizeof(uint32_t) * MEMO_SIZE;
static constexpr size_t FLAG_AND_DEPTH_BYTES = sizeof(uint8_t) * MEMO_SIZE;
static constexpr size_t MEMO_BYTES = TAG_AND_SCORE_BYTES + FLAG_AND_DEPTH_BYTES;


Memo* initMemo() {
    Memo* memo = static_cast<Memo*>(malloc(sizeof(Memo)));
    if (memo == nullptr) return nullptr;

    memo->tagAndScore = static_cast<uint32_t*>(c4Allocate(MEMO_BYTES));
    if (memo->tagAndScore == nullptr) {
        free(memo);
        return nullptr;
    }

    memo->flagAndDepth = reinterpret_cast<uint8_t*>(memo->tagAndScore) + TAG_AND_SCORE_BYTES;
    memset(memo->tagAndScore, 0, MEMO_BYTES);
    return memo;
}


void resetMemo(Memo* memo) {
    if (memo == nullptr) return;

    memset(memo->tagAndScore, 0, MEMO_BYTES);
}


void freeMemo(Memo* memo) {
    if (memo != nullptr) {
        free(static_cast<void*>(memo->tagAndScore));
        free(static_cast<void*>(memo));
    }
}


uint8_t encodeMemoScore(int8_t score, uint8_t occupied) {
    if (score == 0) return 0;

    uint8_t magnitude = static_cast<uint8_t>(score < 0 ? -static_cast<int16_t>(score) : score);
    uint8_t terminalPly = MAX_SCORE - magnitude;
    assert(terminalPly >= occupied && terminalPly <= 42);

    uint8_t distance = terminalPly - occupied;
    assert((score > 0) == ((distance & 1) != 0));
    assert(distance + 1 < (1 << (32 - MEMO_SCORE_SHIFT)));
    return distance + 1;
}


int8_t decodeMemoScore(uint8_t scoreCode, uint8_t occupied) {
    if (scoreCode == 0) return 0;

    uint8_t distance = scoreCode - 1;
    int8_t magnitude = MAX_SCORE - (occupied + distance);
    return (distance & 1) != 0 ? magnitude : -magnitude;
}


int8_t getMemoScore(const Memo& memo, uint32_t slot, uint8_t occupied) {
    return decodeMemoScore(getMemoScoreCode(memo, slot), occupied);
}


bool insertMemoEntry(Memo& memo, uint32_t slot, uint32_t tag, int8_t score,
                     uint8_t occupied, uint8_t depth, int8_t alpha, int8_t beta) {
    // More lookahead wins the slot
    // An empty slot has a depth of 0 and always loses
    // A loaded book entry always loses to a real search
    uint8_t entryDepth = getMemoDepth(memo, slot);
    if (entryDepth != BOOK_DEPTH && depth < entryDepth) return false;

    assert((tag & ~MEMO_TAG_MASK) == 0);
    uint8_t scoreCode = encodeMemoScore(score, occupied);
    memo.tagAndScore[slot] = tag | (static_cast<uint32_t>(scoreCode) << MEMO_SCORE_SHIFT);

    uint8_t flag;
    if (score <= alpha) {
        flag = MEMO_FLAG_UB;
    } else if (score >= beta) {
        flag = MEMO_FLAG_LB;
    } else {
        flag = MEMO_FLAG_EXACT;
    }

    memo.flagAndDepth[slot] = flag | depth;
    return true;
}

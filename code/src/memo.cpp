#include "memo.h"
#include <stdlib.h>




MemoEntry* initMemo() {
    void* mem = malloc(sizeof(MemoEntry) * MEMO_SIZE);
    if (!mem) return nullptr;
    
    MemoEntry* memo = static_cast<MemoEntry*>(mem);
    for (uint32_t i = 0; i < MEMO_SIZE; ++i) {
        memo[i].depth = 0;
    }

    return memo;
}


void resetMemo(MemoEntry* memo) {
    if (memo == nullptr) return;

    for (uint32_t i = 0; i < MEMO_SIZE; ++i) {
        memo[i].depth = 0;
    }
}


void freeMemo(MemoEntry* memo) {
    if (memo != nullptr) {
        free(static_cast<void*>(memo));
    }
}


bool insertMemoEntry(MemoEntry& entry, int8_t score, uint8_t depth, int8_t alpha, int8_t beta) {
    if (depth <= entry.depth) return false;

    entry.score = score;
    entry.depth = depth;

    if (score <= alpha) {
        entry.flag = MEMO_FLAG_UB;
    } else if (score >= beta) {
        entry.flag = MEMO_FLAG_LB;
    } else {
        entry.flag = MEMO_FLAG_EXACT;
    }
}

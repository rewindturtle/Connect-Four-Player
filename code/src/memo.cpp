#include "memo.h"
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>



MemoEntry* initMemo() {
    void* mem = ps_malloc(sizeof(MemoEntry) * MEMO_SIZE);
    if (!mem) return nullptr;

    MemoEntry* memo = static_cast<MemoEntry*>(mem);
    memset(memo, 0, sizeof(MemoEntry) * MEMO_SIZE);

    return memo;
}


void resetMemo(MemoEntry* memo) {
    if (memo == nullptr) return;

    memset(memo, 0, sizeof(MemoEntry) * MEMO_SIZE);
}


void freeMemo(MemoEntry* memo) {
    if (memo != nullptr) {
        free(static_cast<void*>(memo));
    }
}


bool insertMemoEntry(MemoEntry& entry, uint16_t key, int8_t score, uint8_t depth, int8_t alpha, int8_t beta) {
    // Deeper in the tree (smaller depth) wins the slot; equal depths overwrite
    uint8_t entryDepth = getMemoDepth(entry);
    if (entryDepth != 0 && depth > entryDepth) return false;

    entry.key = key;
    entry.score = score;

    uint8_t flag;
    if (score <= alpha) {
        flag = MEMO_FLAG_UB;
    } else if (score >= beta) {
        flag = MEMO_FLAG_LB;
    } else {
        flag = MEMO_FLAG_EXACT;
    }

    entry.flagAndDepth = flag | depth;
    return true;
}

#include "memo.h"
#include <stdlib.h>
#include <Arduino.h>



MemoEntry* initMemo() {
    void* mem = ps_malloc(sizeof(MemoEntry) * MEMO_SIZE);
    if (!mem) return nullptr;

    MemoEntry* memo = static_cast<MemoEntry*>(mem);
    for (uint32_t i = 0; i < MEMO_SIZE; ++i) {
        memo[i].flagAndHorizon = 0;
    }

    return memo;
}


void resetMemo(MemoEntry* memo) {
    if (memo == nullptr) return;

    for (uint32_t i = 0; i < MEMO_SIZE; ++i) {
        memo[i].flagAndHorizon = 0;
    }
}


void freeMemo(MemoEntry* memo) {
    if (memo != nullptr) {
        free(static_cast<void*>(memo));
    }
}


bool insertMemoEntry(MemoEntry& entry, uint16_t key, int8_t score, uint8_t horizon, int8_t alpha, int8_t beta) {
    if (getMemoHorizon(entry) > horizon) return false;

    entry.key = key;
    entry.score = score;

    uint8_t flag;
    if (alpha > score) {
        flag = MEMO_FLAG_UB;
    } else if (beta < score) {
        flag = MEMO_FLAG_LB;
    } else {
        flag = MEMO_FLAG_EXACT;
    }

    entry.flagAndHorizon = flag | horizon;
    return true;
}

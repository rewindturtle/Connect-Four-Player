#include "memo.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>



MemoEntry* initMemo() {
    void* mem = c4Allocate(sizeof(MemoEntry) * MEMO_SIZE);
    if (!mem) return nullptr;

    memset(mem, 0, sizeof(MemoEntry) * MEMO_SIZE);
    MemoEntry* memo = static_cast<MemoEntry*>(mem);

    return memo;
}


void resetMemo(MemoEntry* memo) {
    if (memo == nullptr) return;

    memset(static_cast<void*>(memo), 0, sizeof(MemoEntry) * MEMO_SIZE);
}


void freeMemo(MemoEntry* memo) {
    if (memo != nullptr) {
        free(static_cast<void*>(memo));
    }
}


bool insertMemoEntry(MemoEntry& entry, uint16_t key, int8_t score, uint8_t depth, int8_t alpha, int8_t beta) {
    // More lookahead wins the slot
    // An empty slot has a depth of 0 and always loses
    // A loaded book entry always loses to a real search
    uint8_t entryDepth = getMemoDepth(entry);
    if (entryDepth != BOOK_DEPTH && depth < entryDepth) return false;

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

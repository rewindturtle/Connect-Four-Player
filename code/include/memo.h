#ifndef MEMO_H
#define MEMO_H

#include <stdint.h>

#define MEMO_SIZE 2097152
#define MEMO_BITS 21
#define MEMO_SLOT_MASK 0x001FFFFF

#define MEMO_FLAG_EXACT 0
#define MEMO_FLAG_LB 1
#define MEMO_FLAG_UB 2


struct MemoEntry {
    int8_t score; // Score is between -42 and +42
    uint8_t depth = 0; // Start at depth 1, depth 0 is an unitialized spot
    uint8_t flag;
    uint8_t pad; // Pad to 4 bytes for better alignment
};


MemoEntry* initMemo();
void resetMemo(MemoEntry* memo);
void freeMemo(MemoEntry* memo);

inline bool isSlotInitialized(MemoEntry* memo, uint32_t slot) {
    return memo[slot].depth != 0;
}

bool insertMemoEntry(MemoEntry& entry, int8_t score, uint8_t depth, int8_t alpha, int8_t beta);


#endif // MEMO_H

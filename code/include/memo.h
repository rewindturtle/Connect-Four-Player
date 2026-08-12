#ifndef MEMO_H
#define MEMO_H

#include <stdint.h>

#define MEMO_SIZE 2097152
#define MEMO_BITS 21
#define MEMO_SLOT_MASK 0x001FFFFF

#define DEPTH_MASK 0x3F
#define FLAG_MASK 0xC0

#define MEMO_FLAG_EXACT 0x00
#define MEMO_FLAG_LB 0x40
#define MEMO_FLAG_UB 0x80

#define MIN_SCORE (INT8_MIN + 1)
#define MAX_SCORE INT8_MAX


struct MemoEntry {
    // The position of the entry is equal to (hash & MEMO_SLOT_MASK)
    // The key is the next 16 bits of the hash to check if two colliding
    // hashes are equal. It is possible for two different uint64_t hashes
    // to have the same 16 + MEMO_BITS bits but it is very rare so is allowable
    uint16_t key;

    // Score is between -42 and +42
    int8_t score;

    // The Alpha-beta pruning flag and depth are packed together into 1 byte
    // Depth takes up the first 6 bits while the flag is the back 2
    uint8_t flagAndDepth = 0;
};


inline uint8_t getMemoDepth(const MemoEntry& entry) {
    return entry.flagAndDepth & DEPTH_MASK;
}


inline uint8_t getMemoFlag(const MemoEntry& entry) {
    return entry.flagAndDepth & FLAG_MASK;
}


MemoEntry* initMemo();
void resetMemo(MemoEntry* memo);
void freeMemo(MemoEntry* memo);

inline bool isSlotInitialized(MemoEntry* memo, uint32_t slot) {
    return getMemoDepth(memo[slot]) != 0;
}

bool insertMemoEntry(MemoEntry& entry, uint16_t key, int8_t score, uint8_t depth, int8_t alpha, int8_t beta);


#endif // MEMO_H

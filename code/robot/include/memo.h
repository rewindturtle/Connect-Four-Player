#ifndef MEMO_H
#define MEMO_H

#include <stdint.h>
#include <stddef.h>

#define MEMO_SIZE 2097152
#define MEMO_BITS 21
#define MEMO_SLOT_MASK 0x001FFFFF
#define MEMO_TAG_MASK 0x03FFFFFF
#define MEMO_SCORE_SHIFT 26
#define MEMO_ENTRY_BYTES 5

#define DEPTH_MASK 0x3F
#define FLAG_MASK 0xC0

// Marks a preloaded book entry. Above any real search depth, so the probe
// always accepts it, and insertMemoEntry always lets a real result replace it
#define BOOK_DEPTH DEPTH_MASK

#define MEMO_FLAG_EXACT 0x00
#define MEMO_FLAG_LB 0x40
#define MEMO_FLAG_UB 0x80

#define MIN_SCORE (INT8_MIN + 1)
#define MAX_SCORE INT8_MAX


// Structure-of-arrays keeps the 32-bit values aligned while using exactly five
// bytes per slot. The slot supplies 21 bits of the exact 47-bit board key; the
// remaining 26-bit tag and a 6-bit score code fill tagAndScore.
struct Memo {
    uint32_t* tagAndScore;
    uint8_t* flagAndDepth;
};


inline uint8_t getMemoDepth(const Memo& memo, uint32_t slot) {
    return memo.flagAndDepth[slot] & DEPTH_MASK;
}


inline uint8_t getMemoFlag(const Memo& memo, uint32_t slot) {
    return memo.flagAndDepth[slot] & FLAG_MASK;
}


inline uint32_t getMemoTag(const Memo& memo, uint32_t slot) {
    return memo.tagAndScore[slot] & MEMO_TAG_MASK;
}


inline uint8_t getMemoScoreCode(const Memo& memo, uint32_t slot) {
    return static_cast<uint8_t>(memo.tagAndScore[slot] >> MEMO_SCORE_SHIFT);
}

Memo* initMemo();
void resetMemo(Memo* memo);
void freeMemo(Memo* memo);

inline bool isSlotInitialized(const Memo* memo, uint32_t slot) {
    return getMemoDepth(*memo, slot) != 0;
}

uint8_t encodeMemoScore(int8_t score, uint8_t occupied);
int8_t decodeMemoScore(uint8_t scoreCode, uint8_t occupied);
int8_t getMemoScore(const Memo& memo, uint32_t slot, uint8_t occupied);

bool insertMemoEntry(Memo& memo, uint32_t slot, uint32_t tag, int8_t score,
                     uint8_t occupied, uint8_t depth, int8_t alpha, int8_t beta);


#endif // MEMO_H

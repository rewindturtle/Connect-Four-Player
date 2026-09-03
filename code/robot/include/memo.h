#ifndef MEMO_H
#define MEMO_H

#include <stddef.h>
#include <stdint.h>

#define MEMO_SIZE 2097152
#define MEMO_BITS 21
#define MEMO_SLOT_MASK 0x001FFFFF
#define MEMO_TAG_MASK 0x0FFFFFFF
#define MEMO_SCORE_SHIFT 8
#define MEMO_SCORE_MASK 0x3F00
#define MEMO_ENTRY_BYTES 6

#define DEPTH_MASK 0x3F
#define FLAG_MASK 0xC0

// Marks a preloaded book entry. Above any real search depth, so the probe
// always accepts it, and Memo::insertEntry always lets a real result replace it.
#define BOOK_DEPTH DEPTH_MASK

#define MEMO_FLAG_EXACT 0x00
#define MEMO_FLAG_LB 0x40
#define MEMO_FLAG_UB 0x80

#define MIN_SCORE (INT8_MIN + 1)
#define MAX_SCORE INT8_MAX


// Three naturally aligned 16-bit fields keep each entry at six bytes without
// imposing unaligned 32-bit PSRAM accesses. The table slot supplies 21 bits of
// the exact 49-bit key and tagLow/tagHigh preserve the remaining 28 bits.
struct MemoEntry {
    // Low byte: six-bit depth and two-bit bound flag. High byte: score code.
    uint16_t metadata;
    uint16_t tagLow;
    uint16_t tagHigh;
};

static_assert(sizeof(MemoEntry) == MEMO_ENTRY_BYTES, "MemoEntry must remain six bytes");
static_assert(alignof(MemoEntry) == alignof(uint16_t), "MemoEntry must stay 16-bit aligned");

class Memo {
    private:
        MemoEntry* _entries;
    public:
        Memo();
        ~Memo();

        Memo(const Memo&) = delete;
        Memo& operator=(const Memo&) = delete;
        Memo(Memo&&) = delete;
        Memo& operator=(Memo&&) = delete;

        inline bool isValid() const {return _entries != nullptr;}
        void reset();

        bool probe(uint32_t slot, uint32_t tag, uint8_t occupied, uint8_t depth, int8_t alpha, int8_t beta, int8_t& score) const;
        bool insertEntry(uint32_t slot, uint32_t tag, int8_t score, uint8_t occupied, uint8_t depth, int8_t alpha, int8_t beta);

        inline bool isSlotInitialized(uint32_t slot) const {return getDepth(slot) != 0;}
        inline uint8_t getDepth(uint32_t slot) const {return entryDepth(_entries[slot].metadata);}
        inline uint8_t getFlag(uint32_t slot) const {return entryFlag(_entries[slot].metadata);}
        inline uint32_t getTag(uint32_t slot) const {return entryTag(_entries[slot]);}
        inline uint8_t getScoreCode(uint32_t slot) const { return entryScoreCode(_entries[slot].metadata);}
        int8_t getScore(uint32_t slot, uint8_t occupied) const;

        inline MemoEntry& entry(uint32_t slot) {return _entries[slot];}
        inline const MemoEntry& entry(uint32_t slot) const {return _entries[slot];}
        inline MemoEntry* data() {return _entries;}
        inline const MemoEntry* data() const {return _entries;}

        static inline uint8_t entryDepth(uint16_t metadata) {return static_cast<uint8_t>(metadata & DEPTH_MASK);}
        static inline uint8_t entryFlag(uint16_t metadata) {return static_cast<uint8_t>(metadata & FLAG_MASK);}
        static inline uint8_t entryScoreCode(uint16_t metadata) {return static_cast<uint8_t>((metadata & MEMO_SCORE_MASK) >> MEMO_SCORE_SHIFT);}
        static inline uint32_t entryTag(const MemoEntry& entry) {return static_cast<uint32_t>(entry.tagLow) |(static_cast<uint32_t>(entry.tagHigh & 0x0FFF) << 16);}

        static uint8_t encodeScore(int8_t score, uint8_t occupied);
        static int8_t decodeScore(uint8_t scoreCode, uint8_t occupied);
};

#endif // MEMO_H

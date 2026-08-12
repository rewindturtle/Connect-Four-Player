// Desktop opening-book builder.
//
// Walks every distinct position breadth-first by turn and scores all seven root
// moves at each, which is the query shape the robot's chooseColumn uses. The
// output is the raw memo table plus a small header, ready for an SD card.
//
// The board, hash, entry layout and search below are copies of the firmware's.
// They must stay in step or the book is meaningless -- the fingerprint in the
// header exists to catch that, but it only catches it at load time.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>


// Board

#define COLUMN_MASK 0x3FULL
#define BOARD_MASK 0x0000FDFBF7EFDFBFULL


struct Board {
    uint64_t redPieces;
    uint64_t allPieces;
};


static inline uint64_t getYellowPieces(const Board& board) {
    return board.redPieces ^ board.allPieces;
}


static inline bool isColumnFull(const Board& board, uint8_t col) {
    uint64_t mask = 1ULL << (7 * col + 5);
    return (board.allPieces & mask) != 0;
}


static inline bool isBoardFull(const Board& board) {
    return (board.allPieces & BOARD_MASK) == BOARD_MASK;
}


static bool containsWin(uint64_t pieces) {
    uint64_t m = pieces & (pieces >> 1);
    if (m & (m >> 2)) return true;

    m = pieces & (pieces >> 7);
    if (m & (m >> 14)) return true;

    m = pieces & (pieces >> 8);
    if (m & (m >> 16)) return true;

    m = pieces & (pieces >> 6);
    return (m & (m >> 12)) != 0;
}


static uint64_t landingMask(const Board& board, uint8_t col) {
    uint8_t idx = 7 * col;
    uint64_t colMask = COLUMN_MASK << idx;
    return ((board.allPieces & colMask) + (1ULL << idx)) & colMask;
}


static void placeRedPiece(Board& board, uint8_t col) {
    uint64_t mask = landingMask(board, col);
    board.redPieces |= mask;
    board.allPieces |= mask;
}


static void placeYellowPiece(Board& board, uint8_t col) {
    board.allPieces |= landingMask(board, col);
}


static inline uint64_t murmur3_64(uint64_t key, uint64_t seed) {
    key ^= seed;
    key ^= key >> 33;
    key *= 0xFF51AFD7ED558CCD;
    key ^= key >> 33;
    key *= 0xC4CEB9FE1A85EC53;
    key ^= key >> 33;
    return key;
}


static uint64_t hashBoard(const Board& board) {
    return murmur3_64(board.redPieces, 0x9E3779B97F4A7C15ULL * board.allPieces);
}


// MEMO

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

// Written into every saved entry; above any real search depth
#define BOOK_DEPTH DEPTH_MASK


struct MemoEntry {
    uint16_t key;
    int8_t score;
    uint8_t flagAndDepth;
};

static_assert(sizeof(MemoEntry) == 4, "MemoEntry must stay 4 bytes");


static inline uint8_t getMemoDepth(const MemoEntry& entry) {
    return entry.flagAndDepth & DEPTH_MASK;
}


static inline uint8_t getMemoFlag(const MemoEntry& entry) {
    return entry.flagAndDepth & FLAG_MASK;
}


static bool insertMemoEntry(MemoEntry& entry, uint16_t key, int8_t score, uint8_t depth, int8_t alpha, int8_t beta) {
    // More lookahead wins the slot
    // An empty slot has a depth of 0 and always loses
    if (depth < getMemoDepth(entry)) return false;

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


// SEARCH
static const uint8_t COL_SEARCH_ORDER[7] = {3, 2, 4, 1, 5, 0, 6};

static int8_t negaMaxYellow(const Board& board, MemoEntry* memo, uint8_t depth, int8_t alpha, int8_t beta);


static int8_t negaMaxRed(const Board& board, MemoEntry* memo, uint8_t depth, int8_t alpha, int8_t beta) {
    if (containsWin(getYellowPieces(board))) {
        return -(MAX_SCORE - __builtin_popcountll(board.allPieces));
    } else if (depth == 0 || isBoardFull(board)) {
        return 0;
    }

    int8_t originalAlpha = alpha;
    uint64_t hash = hashBoard(board);
    uint32_t slot = hash & MEMO_SLOT_MASK;
    uint16_t key = (hash >> MEMO_BITS) & 0xFFFF;
    MemoEntry& entry = memo[slot];

    uint8_t slotDepth = getMemoDepth(entry);
    if (slotDepth != 0 && entry.key == key && (slotDepth >= depth || entry.score != 0)) {
        int8_t score = entry.score;

        switch (getMemoFlag(entry)) {
            case MEMO_FLAG_EXACT:
                return score;
            case MEMO_FLAG_LB:
                if (score >= beta) return score;
                break;
            case MEMO_FLAG_UB:
                if (score <= alpha) return score;
                break;
            default:
                break;
        }
    }

    int8_t score = MIN_SCORE;
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (isColumnFull(board, c)) continue;

        Board newBoard = board;
        placeRedPiece(newBoard, c);
        int8_t newScore = -negaMaxYellow(newBoard, memo, depth - 1, -beta, -alpha);

        if (newScore > score) {
            score = newScore;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) break;
            }
        }
    }

    insertMemoEntry(entry, key, score, depth, originalAlpha, beta);
    return score;
}


static int8_t negaMaxYellow(const Board& board, MemoEntry* memo, uint8_t depth, int8_t alpha, int8_t beta) {
    if (containsWin(board.redPieces)) {
        return -(MAX_SCORE - __builtin_popcountll(board.allPieces));
    } else if (depth == 0 || isBoardFull(board)) {
        return 0;
    }

    int8_t originalAlpha = alpha;
    uint64_t hash = hashBoard(board);
    uint32_t slot = hash & MEMO_SLOT_MASK;
    uint16_t key = (hash >> MEMO_BITS) & 0xFFFF;
    MemoEntry& entry = memo[slot];

    uint8_t slotDepth = getMemoDepth(entry);
    if (slotDepth != 0 && entry.key == key && (slotDepth >= depth || entry.score != 0)) {
        int8_t score = entry.score;

        switch (getMemoFlag(entry)) {
            case MEMO_FLAG_EXACT:
                return score;
            case MEMO_FLAG_LB:
                if (score >= beta) return score;
                break;
            case MEMO_FLAG_UB:
                if (score <= alpha) return score;
                break;
            default:
                break;
        }
    }

    int8_t score = MIN_SCORE;
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (isColumnFull(board, c)) continue;

        Board newBoard = board;
        placeYellowPiece(newBoard, c);
        int8_t newScore = -negaMaxRed(newBoard, memo, depth - 1, -beta, -alpha);

        if (newScore > score) {
            score = newScore;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) break;
            }
        }
    }

    insertMemoEntry(entry, key, score, depth, originalAlpha, beta);
    return score;
}


// Mirrors chooseColumn's root loop; the scores are discarded, the table writes
// are the point
static void scoreRootColumns(const Board& board, MemoEntry* memo, uint8_t maxDepth, bool isRed) {
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (isColumnFull(board, c)) continue;

        Board newBoard = board;
        if (isRed) {
            placeRedPiece(newBoard, c);
            negaMaxYellow(newBoard, memo, maxDepth - 1, MIN_SCORE, INT8_MAX);
        } else {
            placeYellowPiece(newBoard, c);
            negaMaxRed(newBoard, memo, maxDepth - 1, MIN_SCORE, INT8_MAX);
        }
    }
}


// BOOK FILE
struct BookHeader {
    uint32_t memoBits;
    uint32_t entrySize;
    uint32_t entryCount;
    uint32_t depth;
    uint32_t turnsDone;
    uint32_t reserved;
    uint64_t fingerprint;
    uint64_t occupied;
};

static_assert(sizeof(BookHeader) == 40, "BookHeader must stay 40 bytes");


// Any change to the hash or board layout moves this, so a stale book is
// refused rather than silently returning wrong scores
static uint64_t hashFingerprint() {
    Board board = {0, 0};
    uint64_t fingerprint = hashBoard(board);

    for (uint8_t c = 0; c < 7; ++c) {
        if (c % 2 == 0) {
            placeRedPiece(board, c);
        } else {
            placeYellowPiece(board, c);
        }

        fingerprint = fingerprint * 0x100000001B3ULL ^ hashBoard(board);
    }

    return fingerprint;
}


static uint64_t countOccupied(const MemoEntry* memo) {
    uint64_t occupied = 0;
    for (uint32_t i = 0; i < MEMO_SIZE; ++i) {
        if (getMemoDepth(memo[i]) != 0) ++occupied;
    }

    return occupied;
}


// Restamped on a copy, never in place: marking the live table would make the
// builder's own replacement policy reject every later insert
static MemoEntry* copyAsBook(const MemoEntry* memo) {
    MemoEntry* book = static_cast<MemoEntry*>(std::malloc(MEMO_SIZE * sizeof(MemoEntry)));
    if (book == nullptr) return nullptr;

    std::memcpy(book, memo, MEMO_SIZE * sizeof(MemoEntry));

    // Empty slots keep depth 0 so the robot still reads them as uninitialized
    for (uint32_t i = 0; i < MEMO_SIZE; ++i) {
        if (getMemoDepth(book[i]) != 0) {
            book[i].flagAndDepth = getMemoFlag(book[i]) | BOOK_DEPTH;
        }
    }

    return book;
}


static bool saveBook(const std::string& path, const MemoEntry* memo, uint32_t depth, uint32_t turnsDone) {
    MemoEntry* book = copyAsBook(memo);
    if (book == nullptr) {
        std::printf("could not allocate the save buffer\n");
        return false;
    }

    BookHeader header;
    std::memset(&header, 0, sizeof(header));
    header.memoBits = MEMO_BITS;
    header.entrySize = sizeof(MemoEntry);
    header.entryCount = MEMO_SIZE;
    header.depth = depth;
    header.turnsDone = turnsDone;
    header.fingerprint = hashFingerprint();
    header.occupied = countOccupied(memo);

    std::string temp = path + ".tmp";
    FILE* file = std::fopen(temp.c_str(), "wb");
    if (file == nullptr) {
        std::printf("could not open %s for writing\n", temp.c_str());
        std::free(book);
        return false;
    }

    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
    ok = ok && std::fwrite(book, sizeof(MemoEntry), MEMO_SIZE, file) == MEMO_SIZE;
    std::fclose(file);
    std::free(book);

    if (!ok) {
        std::printf("write failed, previous book left untouched\n");
        std::remove(temp.c_str());
        return false;
    }

    std::remove(path.c_str());
    return std::rename(temp.c_str(), path.c_str()) == 0;
}


static bool loadBook(const std::string& path, MemoEntry* memo, uint32_t& depth, uint32_t& turnsDone) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return false;

    BookHeader header;
    bool ok = std::fread(&header, sizeof(header), 1, file) == 1;

    if (!ok || header.memoBits != MEMO_BITS || header.entrySize != sizeof(MemoEntry) ||
        header.entryCount != MEMO_SIZE || header.fingerprint != hashFingerprint()) {
        std::printf("%s does not match this table or hash, ignoring it\n", path.c_str());
        std::fclose(file);
        return false;
    }

    ok = std::fread(memo, sizeof(MemoEntry), MEMO_SIZE, file) == MEMO_SIZE;
    std::fclose(file);
    if (!ok) return false;

    depth = header.depth;
    turnsDone = header.turnsDone;
    return true;
}


// -------------------------------------------------------------- driver ----

struct BoardKey {
    uint64_t red;
    uint64_t all;

    bool operator==(const BoardKey& other) const {
        return red == other.red && all == other.all;
    }
};


struct BoardKeyHash {
    size_t operator()(const BoardKey& key) const {
        Board board = {key.red, key.all};
        return static_cast<size_t>(hashBoard(board));
    }
};


static bool isGameOver(const Board& board) {
    return containsWin(board.redPieces) || containsWin(getYellowPieces(board)) || isBoardFull(board);
}


// Every legal position one turn on, with transpositions collapsed
static std::vector<Board> expandFrontier(const std::vector<Board>& frontier, uint32_t turn) {
    std::unordered_set<BoardKey, BoardKeyHash> seen;
    std::vector<Board> next;

    for (size_t i = 0; i < frontier.size(); ++i) {
        const Board& board = frontier[i];

        for (uint8_t c = 0; c < 7; ++c) {
            if (isColumnFull(board, c)) continue;

            Board child = board;
            if (turn % 2 == 0) {
                placeRedPiece(child, c);
            } else {
                placeYellowPiece(child, c);
            }

            if (isGameOver(child)) continue;

            BoardKey key = {child.redPieces, child.allPieces};
            if (seen.insert(key).second) next.push_back(child);
        }
    }

    return next;
}


int main(int argc, char** argv) {
    std::string outPath = "book.bin";
    uint32_t depth = 16;
    uint32_t maxTurn = 10;
    uint32_t saveSeconds = 300;
    bool resume = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        bool hasValue = i + 1 < argc;

        if (arg == "--out" && hasValue) {
            outPath = argv[++i];
        } else if (arg == "--depth" && hasValue) {
            depth = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--max-turn" && hasValue) {
            maxTurn = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--save-seconds" && hasValue) {
            saveSeconds = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--resume") {
            resume = true;
        } else {
            std::printf("usage: %s [--out FILE] [--depth N] [--max-turn N]"
                        " [--save-seconds N] [--resume]\n", argv[0]);
            return 1;
        }
    }

    // Root moves are searched at maxDepth - 1, so past this the search is a no-op
    if (maxTurn + 2 > depth) {
        maxTurn = depth >= 2 ? depth - 2 : 0;
        std::printf("max-turn clamped to %u by depth %u\n", maxTurn, depth);
    }

    MemoEntry* memo = static_cast<MemoEntry*>(std::calloc(MEMO_SIZE, sizeof(MemoEntry)));
    if (memo == nullptr) {
        std::printf("could not allocate %u entries\n", static_cast<unsigned>(MEMO_SIZE));
        return 1;
    }

    uint32_t startTurn = 0;
    if (resume) {
        uint32_t savedDepth = depth;
        if (loadBook(outPath, memo, savedDepth, startTurn)) {
            std::printf("resumed %s at turn %u (depth %u)\n", outPath.c_str(), startTurn, savedDepth);
            depth = savedDepth;
        }
    }

    std::printf("depth %u, turns 0..%u, %u entries, %u byte table\n",
                depth, maxTurn, static_cast<unsigned>(MEMO_SIZE),
                static_cast<unsigned>(MEMO_SIZE * sizeof(MemoEntry)));

    std::vector<Board> frontier;
    frontier.push_back(Board{0, 0});

    auto lastSave = std::chrono::steady_clock::now();
    uint64_t previousOccupied = countOccupied(memo);

    for (uint32_t turn = 0; turn <= maxTurn && !frontier.empty(); ++turn) {
        // Skipping the search still rebuilds the frontier a resumed run needs
        if (turn < startTurn) {
            frontier = expandFrontier(frontier, turn);
            continue;
        }

        auto turnStart = std::chrono::steady_clock::now();
        uint8_t maxDepth = static_cast<uint8_t>(depth - turn);
        bool isRed = (turn % 2 == 0);

        for (size_t i = 0; i < frontier.size(); ++i) {
            scoreRootColumns(frontier[i], memo, maxDepth, isRed);

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastSave).count() >= saveSeconds) {
                saveBook(outPath, memo, depth, turn);
                lastSave = now;
                std::printf("  checkpoint at turn %u, %zu/%zu positions\n", turn, i + 1, frontier.size());
                std::fflush(stdout);
            }
        }

        uint64_t occupied = countOccupied(memo);
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - turnStart).count();

        std::printf("turn %2u  depth %2u  %9zu positions  +%8llu slots  %6.2f%% full  %8.1fs\n",
                    turn, maxDepth, frontier.size(),
                    static_cast<unsigned long long>(occupied - previousOccupied),
                    100.0 * static_cast<double>(occupied) / static_cast<double>(MEMO_SIZE),
                    seconds);
        std::fflush(stdout);

        previousOccupied = occupied;
        saveBook(outPath, memo, depth, turn + 1);
        lastSave = std::chrono::steady_clock::now();

        frontier = expandFrontier(frontier, turn);
    }

    std::printf("wrote %s\n", outPath.c_str());
    std::free(memo);
    return 0;
}

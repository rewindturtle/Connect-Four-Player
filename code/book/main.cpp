// Desktop opening-book builder.
//
// Walks every distinct position breadth-first by turn and scores all seven root
// moves at each, which is the query shape the robot's chooseColumn uses. The
// output is the raw memo table plus a small header, ready for an SD card.
//
// Board keys and memo storage are compiled from the firmware's sources so
// the generated artifact cannot silently drift from the robot's layout.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "board.h"
#include "memo.h"

struct BoardKey {
    uint64_t first;
    uint64_t all;

    bool operator==(const BoardKey& other) const {
        return first == other.first && all == other.all;
    }
};


struct BoardKeyHash {
    size_t operator()(const BoardKey& key) const {
        Board board = {key.first, key.all};
        return static_cast<size_t>(board.getKey());
    }
};


struct CachedEntry {
    int8_t score;
    uint8_t flagAndDepth;
};

static inline uint8_t getCachedDepth(const CachedEntry& entry) {
    return entry.flagAndDepth & DEPTH_MASK;
}


static inline uint8_t getCachedFlag(const CachedEntry& entry) {
    return entry.flagAndDepth & FLAG_MASK;
}


static bool insertCachedEntry(CachedEntry& entry, int8_t score, uint8_t depth, int8_t alpha, int8_t beta) {
    if (depth < getCachedDepth(entry)) return false;

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


// The table is the artifact that ships; the map is an exact, never-evicting
// cache so the desktop stops re-searching whatever the table dropped
struct SearchMemo {
    Memo* table;
    std::unordered_map<BoardKey, CachedEntry, BoardKeyHash> map;
    size_t maxEntries;
    uint8_t minDepth;
};


static bool useCachedEntry(const CachedEntry& entry, uint8_t depth, int8_t alpha, int8_t beta, int8_t& outScore) {
    uint8_t entryDepth = getCachedDepth(entry);
    if (entryDepth == 0) return false;
    if (entryDepth < depth && entry.score == 0) return false;

    outScore = entry.score;

    switch (getCachedFlag(entry)) {
        case MEMO_FLAG_EXACT:
            return true;
        case MEMO_FLAG_LB:
            return entry.score >= beta;
        case MEMO_FLAG_UB:
            return entry.score <= alpha;
        default:
            return false;
    }
}


// Keeps the table populated even when the map short-circuits the search
static void mirrorToTable(Memo& table, uint32_t slot, const CachedEntry& cached,
                          uint32_t tag, uint8_t occupied) {
    MemoEntry& entry = table.entry(slot);
    uint8_t tableDepth = Memo::entryDepth(entry.metadata);
    if (tableDepth != BOOK_DEPTH && getCachedDepth(cached) < tableDepth) return;

    uint8_t scoreCode = Memo::encodeScore(cached.score, occupied);
    entry.tagLow = static_cast<uint16_t>(tag);
    entry.tagHigh = static_cast<uint16_t>(tag >> 16);
    entry.metadata = (static_cast<uint16_t>(scoreCode) << MEMO_SCORE_SHIFT) | cached.flagAndDepth;
}


// Shallow subtrees are cheap to redo, so only deep entries earn a map slot
static void insertMapEntry(SearchMemo& memo, const BoardKey& boardKey, int8_t score, uint8_t depth, int8_t alpha, int8_t beta) {
    if (depth < memo.minDepth) return;

    auto found = memo.map.find(boardKey);
    if (found != memo.map.end()) {
        insertCachedEntry(found->second, score, depth, alpha, beta);
        return;
    }

    if (memo.map.size() >= memo.maxEntries) return;

    CachedEntry entry = {0, 0};
    insertCachedEntry(entry, score, depth, alpha, beta);
    memo.map.emplace(boardKey, entry);
}


// SEARCH
static const uint8_t COL_SEARCH_ORDER[7] = {3, 2, 4, 1, 5, 0, 6};

static int8_t negaMaxSecond(const Board& board, SearchMemo& memo, uint8_t depth, int8_t alpha, int8_t beta);


static int8_t negaMaxFirst(const Board& board, SearchMemo& memo, uint8_t depth, int8_t alpha, int8_t beta) {
    uint8_t occupied = board.getOccupiedCount();
    if (board.secondHasWin()) {
        return -(MAX_SCORE - occupied);
    } else if (depth == 0 || board.isFull()) {
        return 0;
    }

    int8_t originalAlpha = alpha;
    uint64_t key = board.getKey();
    uint32_t slot = key & MEMO_SLOT_MASK;
    uint32_t tag = static_cast<uint32_t>(key >> MEMO_BITS);
    BoardKey boardKey = {board.getFirstPieces(), board.getAllPieces()};
    int8_t cached;

    // The map is exact and never evicts, so it gets asked first
    auto found = memo.map.find(boardKey);
    if (found != memo.map.end()) {
        mirrorToTable(*memo.table, slot, found->second, tag, occupied);
        if (useCachedEntry(found->second, depth, alpha, beta, cached)) return cached;
    }

    if (memo.table->probe(slot, tag, occupied, depth, alpha, beta, cached)) return cached;

    int8_t score = MIN_SCORE;
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (board.isColumnFull(c)) continue;

        Board newBoard = board;
        newBoard.placeFirstPiece(c);
        int8_t newScore = -negaMaxSecond(newBoard, memo, depth - 1, -beta, -alpha);

        if (newScore > score) {
            score = newScore;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) break;
            }
        }
    }

    memo.table->insertEntry(slot, tag, score, occupied, depth, originalAlpha, beta);
    insertMapEntry(memo, boardKey, score, depth, originalAlpha, beta);
    return score;
}


static int8_t negaMaxSecond(const Board& board, SearchMemo& memo, uint8_t depth, int8_t alpha, int8_t beta) {
    uint8_t occupied = board.getOccupiedCount();
    if (board.firstHasWin()) {
        return -(MAX_SCORE - occupied);
    } else if (depth == 0 || board.isFull()) {
        return 0;
    }

    int8_t originalAlpha = alpha;
    uint64_t key = board.getKey();
    uint32_t slot = key & MEMO_SLOT_MASK;
    uint32_t tag = static_cast<uint32_t>(key >> MEMO_BITS);
    BoardKey boardKey = {board.getFirstPieces(), board.getAllPieces()};
    int8_t cached;

    // The map is exact and never evicts, so it gets asked first
    auto found = memo.map.find(boardKey);
    if (found != memo.map.end()) {
        mirrorToTable(*memo.table, slot, found->second, tag, occupied);
        if (useCachedEntry(found->second, depth, alpha, beta, cached)) return cached;
    }

    if (memo.table->probe(slot, tag, occupied, depth, alpha, beta, cached)) return cached;

    int8_t score = MIN_SCORE;
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (board.isColumnFull(c)) continue;

        Board newBoard = board;
        newBoard.placeSecondPiece(c);
        int8_t newScore = -negaMaxFirst(newBoard, memo, depth - 1, -beta, -alpha);

        if (newScore > score) {
            score = newScore;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) break;
            }
        }
    }

    memo.table->insertEntry(slot, tag, score, occupied, depth, originalAlpha, beta);
    insertMapEntry(memo, boardKey, score, depth, originalAlpha, beta);
    return score;
}


// Mirrors chooseColumn's root loop; the scores are discarded, the table writes
// are the point
static void scoreRootColumns(const Board& board, SearchMemo& memo, uint8_t maxDepth, bool firstToMove) {
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (board.isColumnFull(c)) continue;

        Board newBoard = board;
        if (firstToMove) {
            newBoard.placeFirstPiece(c);

            // If a board contains a win, it will not be viewed during normal gameplay
            // so we skip it
            if (newBoard.hasWin()) {
                continue;
            }

            negaMaxSecond(newBoard, memo, maxDepth - 1, MIN_SCORE, INT8_MAX);
        } else {
            newBoard.placeSecondPiece(c);
            if (newBoard.hasWin()) {
                continue;
            }

            negaMaxFirst(newBoard, memo, maxDepth - 1, MIN_SCORE, INT8_MAX);
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
    uint32_t layoutVersion;
    uint64_t fingerprint;
    uint64_t occupied;
};

static_assert(sizeof(BookHeader) == 40, "BookHeader must stay 40 bytes");
static constexpr uint32_t BOOK_LAYOUT_VERSION = 3;


// Any change to the exact key or board layout moves this, so a stale book is
// refused rather than silently returning wrong scores
static uint64_t keyFingerprint() {
    Board board = {0, 0};
    uint64_t fingerprint = board.getKey();

    for (uint8_t c = 0; c < 7; ++c) {
        if (c % 2 == 0) {
            board.placeFirstPiece(c);
        } else {
            board.placeSecondPiece(c);
        }

        fingerprint = fingerprint * 0x100000001B3ULL ^ board.getKey();
    }

    return fingerprint;
}


static uint64_t countOccupied(const Memo& memo) {
    uint64_t occupied = 0;
    for (uint32_t i = 0; i < MEMO_SIZE; ++i) {
        if (memo.isSlotInitialized(i)) ++occupied;
    }

    return occupied;
}


// Restamping a copy keeps the live table's replacement depths intact while
// producing the exact six-byte layout consumed by the robot.
static MemoEntry* copyEntriesAsBook(const Memo& memo) {
    size_t bytes = sizeof(MemoEntry) * MEMO_SIZE;
    MemoEntry* bookEntries = static_cast<MemoEntry*>(std::malloc(bytes));
    if (bookEntries == nullptr) return nullptr;

    std::memcpy(bookEntries, memo.data(), bytes);

    // Empty slots keep depth 0 so the robot still reads them as uninitialized
    for (uint32_t i = 0; i < MEMO_SIZE; ++i) {
        uint16_t metadata = bookEntries[i].metadata;
        if (Memo::entryDepth(metadata) != 0) {
            bookEntries[i].metadata =
                (metadata & static_cast<uint16_t>(~DEPTH_MASK)) | BOOK_DEPTH;
        }
    }

    return bookEntries;
}


static bool saveBook(const std::string& path, const Memo& memo, uint32_t depth, uint32_t turnsDone) {
    MemoEntry* bookEntries = copyEntriesAsBook(memo);
    if (bookEntries == nullptr) {
        std::printf("could not allocate the save buffer\n");
        return false;
    }

    BookHeader header;
    std::memset(&header, 0, sizeof(header));
    header.memoBits = MEMO_BITS;
    header.entrySize = MEMO_ENTRY_BYTES;
    header.entryCount = MEMO_SIZE;
    header.depth = depth;
    header.turnsDone = turnsDone;
    header.layoutVersion = BOOK_LAYOUT_VERSION;
    header.fingerprint = keyFingerprint();
    header.occupied = countOccupied(memo);

    std::string temp = path + ".tmp";
    FILE* file = std::fopen(temp.c_str(), "wb");
    if (file == nullptr) {
        std::printf("could not open %s for writing\n", temp.c_str());
        std::free(bookEntries);
        return false;
    }

    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
    ok = ok && std::fwrite(bookEntries, sizeof(MemoEntry), MEMO_SIZE, file) == MEMO_SIZE;
    std::fclose(file);
    std::free(bookEntries);

    if (!ok) {
        std::printf("write failed, previous book left untouched\n");
        std::remove(temp.c_str());
        return false;
    }

    std::remove(path.c_str());
    return std::rename(temp.c_str(), path.c_str()) == 0;
}


static bool loadBook(const std::string& path, Memo& memo, uint32_t& depth, uint32_t& turnsDone) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return false;

    BookHeader header;
    bool ok = std::fread(&header, sizeof(header), 1, file) == 1;

    if (!ok || header.memoBits != MEMO_BITS || header.entrySize != MEMO_ENTRY_BYTES ||
        header.entryCount != MEMO_SIZE || header.layoutVersion != BOOK_LAYOUT_VERSION ||
        header.fingerprint != keyFingerprint()) {
        std::printf("%s does not match this table or key, ignoring it\n", path.c_str());
        std::fclose(file);
        return false;
    }

    ok = std::fread(memo.data(), sizeof(MemoEntry), MEMO_SIZE, file) == MEMO_SIZE;
    std::fclose(file);
    if (!ok) return false;

    depth = header.depth;
    turnsDone = header.turnsDone;
    return true;
}


// -------------------------------------------------------------- driver ----

static bool isGameOver(const Board& board) {
    return board.hasWin() || board.isFull();
}


// Every legal position one turn on, with transpositions collapsed
static std::vector<Board> expandFrontier(const std::vector<Board>& frontier, uint32_t turn) {
    std::unordered_set<BoardKey, BoardKeyHash> seen;
    std::vector<Board> next;

    for (size_t i = 0; i < frontier.size(); ++i) {
        const Board& board = frontier[i];

        for (uint8_t c = 0; c < 7; ++c) {
            if (board.isColumnFull(c)) continue;

            Board child = board;
            if (turn % 2 == 0) {
                child.placeFirstPiece(c);
            } else {
                child.placeSecondPiece(c);
            }

            if (isGameOver(child)) continue;

            BoardKey key = {child.getFirstPieces(), child.getAllPieces()};
            if (seen.insert(key).second) next.push_back(child);
        }
    }

    return next;
}


int main(int argc, char** argv) {
    std::string outPath = "book.bin";
    uint32_t depth = 20;
    uint32_t maxTurn = 10;
    uint32_t saveSeconds = 300;
    uint32_t mapMinDepth = 6;
    size_t mapMaxEntries = 250000000;
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
        } else if (arg == "--map-min-depth" && hasValue) {
            mapMinDepth = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--map-max-entries" && hasValue) {
            mapMaxEntries = static_cast<size_t>(std::atoll(argv[++i]));
        } else if (arg == "--resume") {
            resume = true;
        } else {
            std::printf("usage: %s [--out FILE] [--depth N] [--max-turn N]"
                        " [--save-seconds N] [--map-min-depth N]"
                        " [--map-max-entries N] [--resume]\n", argv[0]);
            return 1;
        }
    }

    // Root moves are searched at maxDepth - 1, so past this the search is a no-op
    if (maxTurn + 2 > depth) {
        maxTurn = depth >= 2 ? depth - 2 : 0;
        std::printf("max-turn clamped to %u by depth %u\n", maxTurn, depth);
    }

    Memo table;
    if (!table.isValid()) {
        std::printf("could not allocate %u entries\n", static_cast<unsigned>(MEMO_SIZE));
        return 1;
    }

    uint32_t startTurn = 0;
    if (resume) {
        uint32_t savedDepth = depth;
        if (loadBook(outPath, table, savedDepth, startTurn)) {
            std::printf("resumed %s at turn %u (depth %u)\n", outPath.c_str(), startTurn, savedDepth);
            depth = savedDepth;
        }
    }

    SearchMemo memo;
    memo.table = &table;
    memo.maxEntries = mapMaxEntries;
    memo.minDepth = static_cast<uint8_t>(mapMinDepth);

    std::printf("depth %u, turns 0..%u, %u entries, %u byte table\n",
                depth, maxTurn, static_cast<unsigned>(MEMO_SIZE),
                static_cast<unsigned>(MEMO_SIZE * MEMO_ENTRY_BYTES));
    std::printf("map caches depth >= %u, capped at %zu entries\n", mapMinDepth, mapMaxEntries);

    std::vector<Board> frontier;
    frontier.push_back(Board{0, 0});

    auto lastSave = std::chrono::steady_clock::now();
    uint64_t previousOccupied = countOccupied(table);

    for (uint32_t turn = 0; turn <= maxTurn && !frontier.empty(); ++turn) {
        // Skipping the search still rebuilds the frontier a resumed run needs
        if (turn < startTurn) {
            frontier = expandFrontier(frontier, turn);
            continue;
        }

        auto turnStart = std::chrono::steady_clock::now();
        uint8_t maxDepth = static_cast<uint8_t>(depth - turn);
        bool firstToMove = (turn % 2 == 0);

        for (size_t i = 0; i < frontier.size(); ++i) {
            scoreRootColumns(frontier[i], memo, maxDepth, firstToMove);

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastSave).count() >= saveSeconds) {
                saveBook(outPath, table, depth, turn);
                lastSave = now;
                std::printf("  checkpoint at turn %u, %zu/%zu positions\n", turn, i + 1, frontier.size());
                std::fflush(stdout);
            }
        }

        uint64_t occupied = countOccupied(table);
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - turnStart).count();

        std::printf("turn %2u  depth %2u  %9zu positions  +%8llu slots  %6.2f%% full"
                    "  map %7.2fM (~%5.0fMB)  %8.1fs\n",
                    turn, maxDepth, frontier.size(),
                    static_cast<unsigned long long>(occupied - previousOccupied),
                    100.0 * static_cast<double>(occupied) / static_cast<double>(MEMO_SIZE),
                    static_cast<double>(memo.map.size()) / 1e6,
                    static_cast<double>(memo.map.size()) * 56.0 / 1e6,
                    seconds);
        std::fflush(stdout);

        previousOccupied = occupied;
        saveBook(outPath, table, depth, turn + 1);
        lastSave = std::chrono::steady_clock::now();

        frontier = expandFrontier(frontier, turn);
    }

    std::printf("wrote %s\n", outPath.c_str());
    return 0;
}

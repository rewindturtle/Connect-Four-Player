#include "player.h"
#include "platform.h"

#include <string.h>


static const uint8_t COL_SEARCH_ORDER[7] = {3, 2, 4, 1, 5, 0, 6};

// Position of each column within COL_SEARCH_ORDER; lower is more central
static const uint8_t COL_CENTER_RANK[7] = {5, 3, 1, 0, 2, 4, 6};


namespace {
    struct MoveSelection {
        uint8_t column;
        MoveReason reason;
    };
}


Player::Player(Memo* memo) : _memo(memo), _mistakeProb(0.2f),
                             _timeLimitMs(DEFAULT_SEARCH_TIME_LIMIT_MS), _maxDepth(4), _turn(0),
                             _playStyle(STANDARD_PLAY_STYLE), _lastOpponentColumn(NO_COLUMN), _isFirst(true),
                             _isRed(true), _forceStop(false) {}


// Forward declaration
static int8_t negaMaxSecond(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta,
                            uint32_t startMs, uint32_t timeLimitMs);


static inline bool shouldStopSearch(const Player& player, uint32_t startMs, uint32_t timeLimitMs) {
    return player.shouldStop() || (timeLimitMs != 0 && static_cast<uint32_t>(getNow() - startMs) >= timeLimitMs);
}


static int8_t negaMaxFirst(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta,
                           uint32_t startMs, uint32_t timeLimitMs) {
    if (shouldStopSearch(player, startMs, timeLimitMs)) return 0;

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
    int8_t cachedScore;
    if (player.getMemo()->probe(slot, tag, occupied, depth, alpha, beta, cachedScore)) {
        return cachedScore;
    }

    int8_t score = MIN_SCORE;
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (board.isColumnFull(c)) continue;

        Board newBoard = board;
        newBoard.placeFirstPiece(c);
        int8_t newScore = -negaMaxSecond(newBoard, player, depth - 1, -beta, -alpha, startMs, timeLimitMs);
        if (shouldStopSearch(player, startMs, timeLimitMs)) return 0;

        if (newScore > score) {
            score = newScore;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    goto earlyBreak;
                }
            }
        }
    }

    earlyBreak:
    player.getMemo()->insertEntry(slot, tag, score, occupied, depth, originalAlpha, beta);
    return score;
}


static int8_t negaMaxSecond(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta,
                            uint32_t startMs, uint32_t timeLimitMs) {
    if (shouldStopSearch(player, startMs, timeLimitMs)) return 0;

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
    int8_t cachedScore;
    if (player.getMemo()->probe(slot, tag, occupied, depth, alpha, beta, cachedScore)) {
        return cachedScore;
    }

    int8_t score = MIN_SCORE;
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (board.isColumnFull(c)) continue;

        Board newBoard = board;
        newBoard.placeSecondPiece(c);
        int8_t newScore = -negaMaxFirst(newBoard, player, depth - 1, -beta, -alpha, startMs, timeLimitMs);
        if (shouldStopSearch(player, startMs, timeLimitMs)) return 0;

        if (newScore > score) {
            score = newScore;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    goto earlyBreak;
                }
            }
        }
    }

    earlyBreak:
    player.getMemo()->insertEntry(slot, tag, score, occupied, depth, originalAlpha, beta);
    return score;
}


static uint8_t pickRandomColumn(const uint8_t* cols, uint8_t numCols) {
    if (numCols == 1) return cols[0];
    return cols[randomU32() % numCols];
}


static uint8_t collectBestColumns(const int8_t (&scores)[7], uint8_t* cols) {
    uint8_t numCols = 0;
    int8_t highestScore = MIN_SCORE;

    for (uint8_t c = 0; c < 7; ++c) {
        if (scores[c] == MIN_SCORE) continue;

        if (scores[c] > highestScore) {
            highestScore = scores[c];
            cols[0] = c;
            numCols = 1;
        } else if (scores[c] == highestScore) {
            cols[numCols] = c;
            ++numCols;
        }
    }

    return numCols;
}


static MoveSelection chooseColumnStandardStyle(const Player&, const Board&, const int8_t (&scores)[7]) {
    uint8_t cols[7];
    uint8_t numCols = collectBestColumns(scores, cols);

    if (numCols == 0) return {NO_COLUMN, MOVE_REASON_NO_LEGAL_MOVE};
    return {pickRandomColumn(cols, numCols), MOVE_REASON_BEST_SCORE};
}


static MoveSelection chooseColumnMistakeStyle(const Player& player, const Board&, const int8_t (&scores)[7]) {
    uint8_t bestCols[7];
    uint8_t secondBestCols[7];
    uint8_t numBestCols = 0;
    uint8_t numSecondBestCols = 0;
    int8_t highestScore = MIN_SCORE;
    int8_t secondHighestScore = MIN_SCORE;

    for (uint8_t c = 0; c < 7; ++c) {
        if (scores[c] == MIN_SCORE) continue;

        if (scores[c] > highestScore) {
            if (numBestCols != 0) {
                secondHighestScore = highestScore;
                memcpy(secondBestCols, bestCols, sizeof(uint8_t) * numBestCols);
                numSecondBestCols = numBestCols;
            }

            bestCols[0] = c;
            highestScore = scores[c];
            numBestCols = 1;
        } else if (scores[c] == highestScore) {
            bestCols[numBestCols] = c;
            ++numBestCols;
        } else if (scores[c] > secondHighestScore) {
            secondHighestScore = scores[c];
            secondBestCols[0] = c;
            numSecondBestCols = 1;
        } else if (scores[c] == secondHighestScore) {
            secondBestCols[numSecondBestCols] = c;
            ++numSecondBestCols;
        }
    }

    if (numBestCols == 0) return {NO_COLUMN, MOVE_REASON_NO_LEGAL_MOVE};

    float r = static_cast<float>(1. / static_cast<double>(UINT32_MAX)) * static_cast<float>(randomU32());
    if (numSecondBestCols != 0 && r < player.getMistakeProb()) {
        return {pickRandomColumn(secondBestCols, numSecondBestCols), MOVE_REASON_INTENTIONAL_MISTAKE};
    }

    return {pickRandomColumn(bestCols, numBestCols), MOVE_REASON_BEST_SCORE};
}


static MoveSelection chooseColumnProlongStyle(const Player& player, const Board& board, const int8_t (&scores)[7]) {
    uint8_t lowestPositiveCols[7];
    uint8_t zeroCols[7];
    uint8_t numPositiveCols = 0;
    uint8_t numZeroCols = 0;
    int8_t lowestPositiveScore = MAX_SCORE;

    for (uint8_t c = 0; c < 7; ++c) {
        if (scores[c] < 0) continue;

        if (scores[c] == 0) {
            zeroCols[numZeroCols] = c;
            ++numZeroCols;
        } else if (scores[c] < lowestPositiveScore) {
            lowestPositiveScore = scores[c];
            lowestPositiveCols[0] = c;
            numPositiveCols = 1;
        } else if (scores[c] == lowestPositiveScore) {
            lowestPositiveCols[numPositiveCols] = c;
            ++numPositiveCols;
        }
    }

    if (numPositiveCols != 0) {
        return {pickRandomColumn(lowestPositiveCols, numPositiveCols), MOVE_REASON_PROLONG_GAME};
    }
    
    if (numZeroCols != 0) {
        return {pickRandomColumn(zeroCols, numZeroCols), MOVE_REASON_PROLONG_GAME};
    }

    // Every move loses, choose the best one
    return chooseColumnStandardStyle(player, board, scores);
}


// Central picks the most central of the best columns, otherwise the most outer
static MoveSelection chooseColumnByRank(const int8_t (&scores)[7], bool central) {
    uint8_t cols[7];
    uint8_t numCols = collectBestColumns(scores, cols);

    if (numCols == 0) return {NO_COLUMN, MOVE_REASON_NO_LEGAL_MOVE};

    uint8_t best = cols[0];
    for (uint8_t i = 1; i < numCols; ++i) {
        bool better = central ? COL_CENTER_RANK[cols[i]] < COL_CENTER_RANK[best] : COL_CENTER_RANK[cols[i]] > COL_CENTER_RANK[best];
        if (better) best = cols[i];
    }

    MoveReason reason = central ? MOVE_REASON_PREFER_CENTER : MOVE_REASON_PREFER_EDGE;
    return {best, reason};
}


// tallest stacks onto the highest of the best columns, otherwise the lowest
static MoveSelection chooseColumnByHeight(const Board& board, const int8_t (&scores)[7], bool tallest) {
    uint8_t cols[7];
    uint8_t numCols = collectBestColumns(scores, cols);

    if (numCols == 0) return {NO_COLUMN, MOVE_REASON_NO_LEGAL_MOVE};

    uint8_t picked[7];
    uint8_t numPicked = 0;
    uint8_t bestHeight = 0;
    for (uint8_t i = 0; i < numCols; ++i) {
        uint8_t height = board.getColumnHeight(cols[i]);
        bool better = tallest ? height > bestHeight : height < bestHeight;

        if (numPicked == 0 || better) {
            bestHeight = height;
            picked[0] = cols[i];
            numPicked = 1;
        } else if (height == bestHeight) {
            picked[numPicked] = cols[i];
            ++numPicked;
        }
    }

    MoveReason reason = tallest ? MOVE_REASON_STACK_HIGH : MOVE_REASON_SPREAD_LOW;
    return {pickRandomColumn(picked, numPicked), reason};
}


static MoveSelection chooseColumnPacifistStyle(const Player& player, const Board& board, const int8_t (&scores)[7]) {
    uint8_t cols[7];
    uint8_t numCols = 0;

    for (uint8_t c = 0; c < 7; ++c) {
        if (scores[c] != 0) continue;

        cols[numCols] = c;
        ++numCols;
    }

    // Nothing neutral is left, so stop refusing to win
    if (numCols == 0) return chooseColumnStandardStyle(player, board, scores);
    return {pickRandomColumn(cols, numCols), MOVE_REASON_AVOID_WIN};
}


static MoveSelection chooseColumnCopycatStyle(const Player& player, const Board& board, const int8_t (&scores)[7]) {
    if (player.getLastOpponentColumn() >= 7) {
        return chooseColumnStandardStyle(player, board, scores);
    }

    int8_t copyScore = scores[player.getLastOpponentColumn()];

    // A full column scores MIN_SCORE, so it drops out here too
    if (copyScore < 0) return chooseColumnStandardStyle(player, board, scores);

    // Never copy a neutral move when a winning one is on the board
    if (copyScore == 0) {
        for (uint8_t c = 0; c < 7; ++c) {
            if (scores[c] > 0) return chooseColumnStandardStyle(player, board, scores);
        }
    }

    return {player.getLastOpponentColumn(), MOVE_REASON_COPY_OPPONENT};
}


// Counts opponent replies to col that are not already losing for the opponent.
// False means the analysis was interrupted and its partial count is unusable.
static bool countSafeReplies(const Player& player, const Board& board, uint8_t col, uint8_t depth,
                             uint32_t startMs, uint32_t timeLimitMs, uint8_t& safeReplies) {
    Board afterMove = board;
    if (player.isFirst()) {
        afterMove.placeFirstPiece(col);
    } else {
        afterMove.placeSecondPiece(col);
    }

    safeReplies = 0;
    for (uint8_t r = 0; r < 7; ++r) {
        if (afterMove.isColumnFull(r)) continue;
        if (shouldStopSearch(player, startMs, timeLimitMs)) return false;

        Board afterReply = afterMove;
        int8_t score;
        if (player.isFirst()) {
            afterReply.placeSecondPiece(r);
            score = negaMaxFirst(afterReply, player, depth, MIN_SCORE, MAX_SCORE, startMs, timeLimitMs);
        } else {
            afterReply.placeFirstPiece(r);
            score = negaMaxSecond(afterReply, player, depth, MIN_SCORE, MAX_SCORE, startMs, timeLimitMs);
        }
        if (shouldStopSearch(player, startMs, timeLimitMs)) return false;

        if (score <= 0) {
            ++safeReplies;
        }
    }

    return true;
}


static MoveSelection chooseColumnTrapStyle(const Player& player, const Board& board, const int8_t (&scores)[7],
                                           uint8_t completedDepth, uint32_t startMs, uint32_t timeLimitMs) {
    uint8_t cols[7];
    uint8_t numCols = collectBestColumns(scores, cols);

    if (numCols == 0) return {NO_COLUMN, MOVE_REASON_NO_LEGAL_MOVE};

    // A forced win or loss is already decided, so only break neutral ties
    if (scores[cols[0]] != 0) return chooseColumnStandardStyle(player, board, scores);

    // Trap analysis includes our move and the opponent's reply, so at least
    // three completed plies are needed to keep it inside the search horizon.
    if (completedDepth < 3 || shouldStopSearch(player, startMs, timeLimitMs)) {
        return chooseColumnStandardStyle(player, board, scores);
    }
    uint8_t replyDepth = completedDepth - 2;

    uint8_t picked[7];
    uint8_t numPicked = 0;
    uint8_t fewestSafe = 0;
    for (uint8_t i = 0; i < numCols; ++i) {
        uint8_t safeReplies;
        if (!countSafeReplies(player, board, cols[i], replyDepth, startMs, timeLimitMs, safeReplies)) {
            return chooseColumnStandardStyle(player, board, scores);
        }

        if (numPicked == 0 || safeReplies < fewestSafe) {
            fewestSafe = safeReplies;
            picked[0] = cols[i];
            numPicked = 1;
        } else if (safeReplies == fewestSafe) {
            picked[numPicked] = cols[i];
            ++numPicked;
        }
    }

    return {pickRandomColumn(picked, numPicked), MOVE_REASON_CREATE_TRAP};
}


static void initializeLegalScores(const Board& board, int8_t (&scores)[7]) {
    for (uint8_t c = 0; c < 7; ++c) {
        scores[c] = board.isColumnFull(c) ? MIN_SCORE : 0;
    }
}


// Returns true only if every legal column completed at this depth.
static bool scoreColumns(const Player& player, const Board& board, uint8_t maxDepth, int8_t (&scores)[7],
                         uint32_t startMs, uint32_t timeLimitMs) {
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (board.isColumnFull(c)) {
            scores[c] = MIN_SCORE;
            continue;
        }
        if (shouldStopSearch(player, startMs, timeLimitMs)) return false;

        Board newBoard = board;
        if (player.isFirst()) {
            newBoard.placeFirstPiece(c);
            scores[c] = -negaMaxSecond(newBoard, player, maxDepth - 1, MIN_SCORE, MAX_SCORE,
                                       startMs, timeLimitMs);
        } else {
            newBoard.placeSecondPiece(c);
            scores[c] = -negaMaxFirst(newBoard, player, maxDepth - 1, MIN_SCORE, MAX_SCORE,
                                      startMs, timeLimitMs);
        }
        if (shouldStopSearch(player, startMs, timeLimitMs)) return false;
    }

    return true;
}


MoveDecision Player::chooseMove(const Board& board) const {
    int8_t scores[7];
    initializeLegalScores(board, scores);

    uint8_t remainingMoves = 42 - board.getOccupiedCount();
    uint8_t maxSearchDepth = getMaxDepth() < remainingMoves ? getMaxDepth() : remainingMoves;
    uint8_t completedDepth = 0;
    uint32_t startMs = getNow();
    uint32_t timeLimitMs = getTimeLimitMs();

    if (maxSearchDepth != 0) {
        uint8_t depth = maxSearchDepth < INITIAL_SEARCH_DEPTH ? maxSearchDepth : INITIAL_SEARCH_DEPTH;
        while (depth <= maxSearchDepth && !shouldStopSearch(*this, startMs, timeLimitMs)) {
            int8_t iterationScores[7];
            if (!scoreColumns(*this, board, depth, iterationScores, startMs, timeLimitMs)) break;

            memcpy(scores, iterationScores, sizeof(scores));
            completedDepth = depth;
            if (depth == maxSearchDepth) break;
            ++depth;
        }
    }

    MoveSelection selection;
    switch (getPlayStyle()) {
        case MISTAKES_PLAY_STYLE:
            selection = chooseColumnMistakeStyle(*this, board, scores);
            break;
        case PROLONG_PLAY_STYLE:
            selection = chooseColumnProlongStyle(*this, board, scores);
            break;
        case CENTER_PLAY_STYLE:
            selection = chooseColumnByRank(scores, true);
            break;
        case EDGE_PLAY_STYLE:
            selection = chooseColumnByRank(scores, false);
            break;
        case STACKER_PLAY_STYLE:
            selection = chooseColumnByHeight(board, scores, true);
            break;
        case SPREADER_PLAY_STYLE:
            selection = chooseColumnByHeight(board, scores, false);
            break;
        case PACIFIST_PLAY_STYLE:
            selection = chooseColumnPacifistStyle(*this, board, scores);
            break;
        case COPYCAT_PLAY_STYLE:
            selection = chooseColumnCopycatStyle(*this, board, scores);
            break;
        case TRAP_PLAY_STYLE:
            selection = chooseColumnTrapStyle(*this, board, scores, completedDepth, startMs, timeLimitMs);
            break;
        default:
            selection = chooseColumnStandardStyle(*this, board, scores);
            break;
    }

    int8_t bestScore = MIN_SCORE;
    for (uint8_t c = 0; c < 7; ++c) {
        if (scores[c] > bestScore) bestScore = scores[c];
    }

    int8_t score = selection.column < 7 ? scores[selection.column] : MIN_SCORE;
    return {selection.column, score, bestScore, selection.reason};
}


uint8_t Player::chooseColumn(const Board& board) const {
    return chooseMove(board).column;
}


void Player::idleSearch(const Board& board) const {
    uint8_t remainingMoves = 42 - board.getOccupiedCount();
    uint8_t maxSearchDepth = getMaxDepth() < remainingMoves ? getMaxDepth() : remainingMoves;
    if (maxSearchDepth == 0) return;

    // No deadline: idle search ends only at max depth or via setForceStop().
    uint8_t depth = maxSearchDepth < INITIAL_SEARCH_DEPTH ? maxSearchDepth : INITIAL_SEARCH_DEPTH;
    while (depth <= maxSearchDepth && !shouldStop()) {
        if (isFirst()) {
            negaMaxSecond(board, *this, depth, MIN_SCORE, MAX_SCORE, 0, 0);
        } else {
            negaMaxFirst(board, *this, depth, MIN_SCORE, MAX_SCORE, 0, 0);
        }

        if (depth == maxSearchDepth) break;
        ++depth;
    }
}

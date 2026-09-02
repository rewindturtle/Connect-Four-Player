#include "player.h"
#include "platform.h"

#include <string.h>


static const uint8_t COL_SEARCH_ORDER[7] = {3, 2, 4, 1, 5, 0, 6};

// Position of each column within COL_SEARCH_ORDER; lower is more central
static const uint8_t COL_CENTER_RANK[7] = {5, 3, 1, 0, 2, 4, 6};


struct MoveSelection {
    uint8_t column;
    MoveReason reason;
};


// Forward declaration
int8_t negaMaxSecond(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta);


static bool useMemoEntry(const Memo& memo, uint32_t slot, uint32_t tag, uint8_t occupied,
                         uint8_t depth, int8_t alpha, int8_t beta, int8_t& score) {
    uint8_t slotDepth = getMemoDepth(memo, slot);
    if (slotDepth == 0 || getMemoTag(memo, slot) != tag) return false;

    score = getMemoScore(memo, slot, occupied);

    // A non-zero score is a proven win or loss, which no deeper search can change.
    if (slotDepth < depth && score == 0) return false;

    switch (getMemoFlag(memo, slot)) {
        case MEMO_FLAG_EXACT:
            return true;
        case MEMO_FLAG_LB:
            return score >= beta;
        case MEMO_FLAG_UB:
            return score <= alpha;
        default:
            return false;
    }
}


int8_t negaMaxFirst(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta) {
    uint8_t occupied = static_cast<uint8_t>(__builtin_popcountll(board.allPieces));
    if (containsWin(getSecondPieces(board))) {
        return -(MAX_SCORE - occupied);
    } else if (depth == 0 || isBoardFull(board)) {
        return 0;
    }

    int8_t originalAlpha = alpha;
    uint64_t key = getBoardKey(board);
    uint32_t slot = key & MEMO_SLOT_MASK;
    uint32_t tag = static_cast<uint32_t>(key >> MEMO_BITS);
    int8_t cachedScore;
    if (useMemoEntry(*player.memo, slot, tag, occupied, depth, alpha, beta, cachedScore)) {
        return cachedScore;
    }

    int8_t score = MIN_SCORE;
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (isColumnFull(board, c)) continue;

        Board newBoard = board;
        placeFirstPiece(newBoard, c);
        int8_t newScore = -negaMaxSecond(newBoard, player, depth - 1, -beta, -alpha);
        if (player.forceStop) return 0;

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
    insertMemoEntry(*player.memo, slot, tag, score, occupied, depth, originalAlpha, beta);
    return score;
}


int8_t negaMaxSecond(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta) {
    uint8_t occupied = static_cast<uint8_t>(__builtin_popcountll(board.allPieces));
    if (containsWin(board.firstPieces)) {
        return -(MAX_SCORE - occupied);
    } else if (depth == 0 || isBoardFull(board)) {
        return 0;
    }

    int8_t originalAlpha = alpha;
    uint64_t key = getBoardKey(board);
    uint32_t slot = key & MEMO_SLOT_MASK;
    uint32_t tag = static_cast<uint32_t>(key >> MEMO_BITS);
    int8_t cachedScore;
    if (useMemoEntry(*player.memo, slot, tag, occupied, depth, alpha, beta, cachedScore)) {
        return cachedScore;
    }

    int8_t score = MIN_SCORE;
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (isColumnFull(board, c)) continue;

        Board newBoard = board;
        placeSecondPiece(newBoard, c);
        int8_t newScore = -negaMaxFirst(newBoard, player, depth - 1, -beta, -alpha);
        if (player.forceStop) return 0;

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
    insertMemoEntry(*player.memo, slot, tag, score, occupied, depth, originalAlpha, beta);
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
    if (numSecondBestCols != 0 && r < player.mistakeProb) {
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
        uint8_t height = getColumnHeight(board, cols[i]);
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
    if (player.lastOpponentColumn >= 7) {
        return chooseColumnStandardStyle(player, board, scores);
    }

    int8_t copyScore = scores[player.lastOpponentColumn];

    // A full column scores MIN_SCORE, so it drops out here too
    if (copyScore < 0) return chooseColumnStandardStyle(player, board, scores);

    // Never copy a neutral move when a winning one is on the board
    if (copyScore == 0) {
        for (uint8_t c = 0; c < 7; ++c) {
            if (scores[c] > 0) return chooseColumnStandardStyle(player, board, scores);
        }
    }

    return {player.lastOpponentColumn, MOVE_REASON_COPY_OPPONENT};
}


// Counts opponent replies to col that are not already losing for the opponent
static uint8_t countSafeReplies(const Player& player, const Board& board, uint8_t col, uint8_t depth) {
    Board afterMove = board;
    if (player.isFirst) {
        placeFirstPiece(afterMove, col);
    } else {
        placeSecondPiece(afterMove, col);
    }

    uint8_t safeReplies = 0;
    for (uint8_t r = 0; r < 7; ++r) {
        if (isColumnFull(afterMove, r)) continue;

        Board afterReply = afterMove;
        int8_t score;
        if (player.isFirst) {
            placeSecondPiece(afterReply, r);
            score = negaMaxFirst(afterReply, player, depth, MIN_SCORE, MAX_SCORE);
        } else {
            placeFirstPiece(afterReply, r);
            score = negaMaxSecond(afterReply, player, depth, MIN_SCORE, MAX_SCORE);
        }

        if (score <= 0) {
            ++safeReplies;
        }
    }

    return safeReplies;
}


static MoveSelection chooseColumnTrapStyle(const Player& player, const Board& board, const int8_t (&scores)[7]) {
    uint8_t cols[7];
    uint8_t numCols = collectBestColumns(scores, cols);

    if (numCols == 0) return {NO_COLUMN, MOVE_REASON_NO_LEGAL_MOVE};

    // A forced win or loss is already decided, so only break neutral ties
    if (scores[cols[0]] != 0) return chooseColumnStandardStyle(player, board, scores);

    uint8_t replyDepth = player.maxDepth > 2 ? player.maxDepth - 2 : 1;

    uint8_t picked[7];
    uint8_t numPicked = 0;
    uint8_t fewestSafe = 0;
    for (uint8_t i = 0; i < numCols; ++i) {
        uint8_t safeReplies = countSafeReplies(player, board, cols[i], replyDepth);

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


static void scoreColumns(const Player& player, const Board& board, uint8_t maxDepth, int8_t (&scores)[7]) {
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (isColumnFull(board, c)) {
            scores[c] = MIN_SCORE;
            continue;
        }

        Board newBoard = board;
        if (player.isFirst) {
            placeFirstPiece(newBoard, c);
            scores[c] = -negaMaxSecond(newBoard, player, maxDepth - 1, MIN_SCORE, MAX_SCORE);
        } else {
            placeSecondPiece(newBoard, c);
            scores[c] = -negaMaxFirst(newBoard, player, maxDepth - 1, MIN_SCORE, MAX_SCORE);
        }
    }
}


// A visible loss is worth a second, deeper look
static bool isThreatened(const int8_t (&scores)[7]) {
    for (uint8_t c = 0; c < 7; ++c) {
        if (scores[c] < 0 && scores[c] != MIN_SCORE) return true;
    }

    return false;
}


MoveDecision chooseMove(const Player& player, const Board& board) {
    int8_t scores[7];
    scoreColumns(player, board, player.maxDepth, scores);

    if (player.canPanic && isThreatened(scores)) {
        scoreColumns(player, board, player.panicDepth, scores);
    }

    MoveSelection selection;
    switch (player.playStyle) {
        case MISTAKES_PLAY_STYLE:
            selection = chooseColumnMistakeStyle(player, board, scores);
            break;
        case PROLONG_PLAY_STYLE:
            selection = chooseColumnProlongStyle(player, board, scores);
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
            selection = chooseColumnPacifistStyle(player, board, scores);
            break;
        case COPYCAT_PLAY_STYLE:
            selection = chooseColumnCopycatStyle(player, board, scores);
            break;
        case TRAP_PLAY_STYLE:
            selection = chooseColumnTrapStyle(player, board, scores);
            break;
        default:
            selection = chooseColumnStandardStyle(player, board, scores);
            break;
    }

    int8_t bestScore = MIN_SCORE;
    for (uint8_t c = 0; c < 7; ++c) {
        if (scores[c] > bestScore) bestScore = scores[c];
    }

    int8_t score = selection.column < 7 ? scores[selection.column] : MIN_SCORE;
    return {selection.column, score, bestScore, selection.reason};
}


uint8_t chooseColumn(const Player& player, const Board& board) {
    return chooseMove(player, board).column;
}


void idleSearch(const Player& player, const Board& board) {
    uint8_t maxSearchDepth = 42 - player.turn;
    for (uint8_t i = 1; i <= maxSearchDepth && !player.forceStop; ++i) {
        if (player.isFirst) {
            negaMaxSecond(board, player, i, MIN_SCORE, MAX_SCORE);
        } else {
            negaMaxFirst(board, player, i, MIN_SCORE, MAX_SCORE);
        }
    }
}

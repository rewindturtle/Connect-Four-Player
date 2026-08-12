#include "player.h"
#include <esp_random.h>
#include <string.h>


static const uint8_t COL_SEARCH_ORDER[7] = {3, 2, 4, 1, 5, 0, 6};

// Position of each column within COL_SEARCH_ORDER; lower is more central
static const uint8_t COL_CENTER_RANK[7] = {5, 3, 1, 0, 2, 4, 6};


// Forward Declaration
int8_t negaMaxYellow(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta);


int8_t negaMaxRed(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta) {
    if (containsWin(getYellowPieces(board))) {
        return -(MAX_SCORE - __builtin_popcountll(board.allPieces));
    } else if (depth == 0 || isBoardFull(board)) {
        return 0;
    }
    
    int8_t originalAlpha = alpha;
    uint64_t hash = hashBoard(board);
    uint32_t slot = hash & MEMO_SLOT_MASK;
    uint16_t key = (hash >> MEMO_BITS) & 0xFFFF;
    MemoEntry& entry = player.memo[slot];

    // A non-zero score is a proven win or loss, which no deeper search can change
    uint8_t slotDepth = getMemoDepth(entry);
    if (slotDepth != 0 && entry.key == key && (slotDepth >= depth || entry.score != 0)) {
        int8_t score = entry.score;
        uint8_t flag = getMemoFlag(entry);

        switch (flag) {
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
        int8_t newScore = -negaMaxYellow(newBoard, player, depth - 1, -beta, -alpha);
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
    insertMemoEntry(entry, key, score, depth, originalAlpha, beta);
    return score;
}


int8_t negaMaxYellow(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta) {
    if (containsWin(board.redPieces)) {
        return -(MAX_SCORE - __builtin_popcountll(board.allPieces));
    } else if (depth == 0 || isBoardFull(board)) {
        return 0;
    }

    int8_t originalAlpha = alpha;
    uint64_t hash = hashBoard(board);
    uint32_t slot = hash & MEMO_SLOT_MASK;
    uint16_t key = static_cast<uint16_t>(hash >> MEMO_BITS);
    MemoEntry& entry = player.memo[slot];

    // A non-zero score is a proven win or loss, which no deeper search can change
    uint8_t slotDepth = getMemoDepth(entry);
    if (slotDepth != 0 && entry.key == key && (slotDepth >= depth || entry.score != 0)) {
        int8_t score = entry.score;
        uint8_t flag = getMemoFlag(entry);

        switch (flag) {
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
        int8_t newScore = -negaMaxRed(newBoard, player, depth - 1, -beta, -alpha);
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
    insertMemoEntry(entry, key, score, depth, originalAlpha, beta);
    return score;
}


static uint8_t pickRandomColumn(const uint8_t* cols, uint8_t numCols) {
    if (numCols == 1) return cols[0];
    return cols[esp_random() % numCols];
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


static uint8_t chooseColumnStandardStyle(const Player&, const Board&, const int8_t (&scores)[7]) {
    uint8_t cols[7];
    uint8_t numCols = collectBestColumns(scores, cols);

    if (numCols == 0) return 0;
    return pickRandomColumn(cols, numCols);
}


static uint8_t chooseColumnMistakeStyle(const Player& player, const Board&, const int8_t (&scores)[7]) {
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

    if (numBestCols == 0) return 0;

    float r = static_cast<float>(1. / static_cast<double>(UINT32_MAX)) * static_cast<float>(esp_random());
    if (numSecondBestCols != 0 && r < player.mistakeProb) {
        return pickRandomColumn(secondBestCols, numSecondBestCols);
    }

    return pickRandomColumn(bestCols, numBestCols);
}


static uint8_t chooseColumnProlongStyle(const Player& player, const Board& board, const int8_t (&scores)[7]) {
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

    if (numPositiveCols != 0) return pickRandomColumn(lowestPositiveCols, numPositiveCols);
    if (numZeroCols != 0) return pickRandomColumn(zeroCols, numZeroCols);

    // Every move loses, choose the best one
    return chooseColumnStandardStyle(player, board, scores);
}


// Central picks the most central of the best columns, otherwise the most outer
static uint8_t chooseColumnByRank(const int8_t (&scores)[7], bool central) {
    uint8_t cols[7];
    uint8_t numCols = collectBestColumns(scores, cols);

    if (numCols == 0) return 0;

    uint8_t best = cols[0];
    for (uint8_t i = 1; i < numCols; ++i) {
        bool better = central ? COL_CENTER_RANK[cols[i]] < COL_CENTER_RANK[best] : COL_CENTER_RANK[cols[i]] > COL_CENTER_RANK[best];
        if (better) best = cols[i];
    }

    return best;
}


// tallest stacks onto the highest of the best columns, otherwise the lowest
static uint8_t chooseColumnByHeight(const Board& board, const int8_t (&scores)[7], bool tallest) {
    uint8_t cols[7];
    uint8_t numCols = collectBestColumns(scores, cols);

    if (numCols == 0) return 0;

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

    return pickRandomColumn(picked, numPicked);
}


static uint8_t chooseColumnPacifistStyle(const Player& player, const Board& board, const int8_t (&scores)[7]) {
    uint8_t cols[7];
    uint8_t numCols = 0;

    for (uint8_t c = 0; c < 7; ++c) {
        if (scores[c] != 0) continue;

        cols[numCols] = c;
        ++numCols;
    }

    // Nothing neutral is left, so stop refusing to win
    if (numCols == 0) return chooseColumnStandardStyle(player, board, scores);
    return pickRandomColumn(cols, numCols);
}


static uint8_t chooseColumnCopycatStyle(const Player& player, const Board& board, const int8_t (&scores)[7]) {
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

    return player.lastOpponentColumn;
}


// Counts opponent replies to col that are not already losing for the opponent
static uint8_t countSafeReplies(const Player& player, const Board& board, uint8_t col, uint8_t depth) {
    Board afterMove = board;
    if (player.isRed) {
        placeRedPiece(afterMove, col);
    } else {
        placeYellowPiece(afterMove, col);
    }

    uint8_t safeReplies = 0;
    for (uint8_t r = 0; r < 7; ++r) {
        if (isColumnFull(afterMove, r)) continue;

        Board afterReply = afterMove;
        int8_t score;
        if (player.isRed) {
            placeYellowPiece(afterReply, r);
            score = negaMaxRed(afterReply, player, depth, MIN_SCORE, MAX_SCORE);
        } else {
            placeRedPiece(afterReply, r);
            score = negaMaxYellow(afterReply, player, depth, MIN_SCORE, MAX_SCORE);
        }

        if (score <= 0) {
            ++safeReplies;
        }
    }

    return safeReplies;
}


static uint8_t chooseColumnTrapStyle(const Player& player, const Board& board, const int8_t (&scores)[7]) {
    uint8_t cols[7];
    uint8_t numCols = collectBestColumns(scores, cols);

    if (numCols == 0) return 0;

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

    return pickRandomColumn(picked, numPicked);
}


static void scoreColumns(const Player& player, const Board& board, uint8_t maxDepth, int8_t (&scores)[7]) {
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (isColumnFull(board, c)) {
            scores[c] = MIN_SCORE;
            continue;
        }

        Board newBoard = board;
        if (player.isRed) {
            placeRedPiece(newBoard, c);
            scores[c] = -negaMaxYellow(newBoard, player, maxDepth - 1, MIN_SCORE, MAX_SCORE);
        } else {
            placeYellowPiece(newBoard, c);
            scores[c] = -negaMaxRed(newBoard, player, maxDepth - 1, MIN_SCORE, MAX_SCORE);
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


uint8_t chooseColumn(const Player& player, const Board& board) {
    int8_t scores[7];
    scoreColumns(player, board, player.maxDepth, scores);

    if (player.canPanic && isThreatened(scores)) {
        scoreColumns(player, board, player.panicDepth, scores);
    }

    switch (player.playStyle) {
        case MISTAKES_PLAY_STYLE:
            return chooseColumnMistakeStyle(player, board, scores);
        case PROLONG_PLAY_STYLE:
            return chooseColumnProlongStyle(player, board, scores);
        case CENTER_PLAY_STYLE:
            return chooseColumnByRank(scores, true);
        case EDGE_PLAY_STYLE:
            return chooseColumnByRank(scores, false);
        case STACKER_PLAY_STYLE:
            return chooseColumnByHeight(board, scores, true);
        case SPREADER_PLAY_STYLE:
            return chooseColumnByHeight(board, scores, false);
        case PACIFIST_PLAY_STYLE:
            return chooseColumnPacifistStyle(player, board, scores);
        case COPYCAT_PLAY_STYLE:
            return chooseColumnCopycatStyle(player, board, scores);
        case TRAP_PLAY_STYLE:
            return chooseColumnTrapStyle(player, board, scores);
        default:
            return chooseColumnStandardStyle(player, board, scores);
    }
}


void idleSearch(const Player& player, const Board& board) {
    uint8_t maxSearchDepth = 42 - player.turn;
    for (uint8_t i = 1; i <= maxSearchDepth && !player.forceStop; ++i) {
        if (player.isRed) {
            negaMaxYellow(board, player, i, MIN_SCORE, MAX_SCORE);
        } else {
            negaMaxRed(board, player, i, MIN_SCORE, MAX_SCORE);
        }
    }
}

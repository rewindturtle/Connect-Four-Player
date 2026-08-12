#include "player.h"
#include <esp_random.h>
#include <string.h>


static const uint8_t COL_SEARCH_ORDER[7] = {3, 2, 4, 1, 5, 0, 6};


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


static uint8_t chooseColumnStandardStyle(const int8_t (&scores)[7]) {
    uint8_t cols[7];
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

    if (numCols == 0) return 0;
    return pickRandomColumn(cols, numCols);
}


static uint8_t chooseColumnMistakeStyle(const int8_t (&scores)[7], float mistakeProb) {
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
    if (numSecondBestCols != 0 && r < mistakeProb) {
        return pickRandomColumn(secondBestCols, numSecondBestCols);
    }

    return pickRandomColumn(bestCols, numBestCols);
}


static uint8_t chooseColumnProlongStyle(const int8_t (&scores)[7]) {
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
    return chooseColumnStandardStyle(scores);
}


uint8_t chooseColumn(const Player& player, const Board& board) {
    int8_t scores[7];
    for (uint8_t i = 0; i < 7; ++i) {
        uint8_t c = COL_SEARCH_ORDER[i];

        if (isColumnFull(board, c)) {
            scores[c] = MIN_SCORE;
        } else {
            Board newBoard = board;
            if (player.isRed) {
                placeRedPiece(newBoard, c);
                scores[c] = -negaMaxYellow(newBoard, player, player.maxDepth - 1, MIN_SCORE, INT8_MAX);
            } else {
                placeYellowPiece(newBoard, c);
                scores[c] = -negaMaxRed(newBoard, player, player.maxDepth - 1, MIN_SCORE, INT8_MAX);
            }
        }
    }

    switch (player.playStyle) {
        case MISTAKES_PLAY_STYLE:
            return chooseColumnMistakeStyle(scores, player.mistakeProb);
        case PROLONG_PLAY_STYLE:
            return chooseColumnProlongStyle(scores);
        default:
            return chooseColumnStandardStyle(scores);
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

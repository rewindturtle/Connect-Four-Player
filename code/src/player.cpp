#include "player.h"
#include <stdlib.h>


int8_t negaMaxRed(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta) {
    int8_t originalAlpha = alpha;

    uint32_t slot = hashBoard(board) & MEMO_SLOT_MASK;
    MemoEntry& entry = player.memo[slot];

    uint8_t slotDepth = entry.depth;
    if (slotDepth != 0 && slotDepth < depth) {
        int8_t score = entry.score;
        uint8_t flag = entry.flag;

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

    if (containsWin(board.redPieces)) {
        return INT8_MAX - depth;
    }
    
    if (depth == player.maxDepth || isBoardFull(board)) {
        return 0;
    }

    int8_t score = INT8_MIN;
    for (uint8_t c = 0; c < 7; ++c) {
        if (isColumnFull(board, c)) continue;

        Board newBoard = board;
        placeRedPiece(newBoard, c);
        int8_t newScore = -negaMaxYellow(newBoard, player, depth + 1, -beta, -alpha);
        
        bool breakLoop = false;
        if (newScore > score) {
            score = newScore;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    breakLoop = true;
                }
            }
        }

        if (breakLoop) break;
    }

    insertMemoEntry(entry, score, player.turn + depth, originalAlpha, beta);
    return score;
}


int8_t negaMaxYellow(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta) {
    int8_t originalAlpha = alpha;

    uint32_t slot = hashBoard(board) & MEMO_SLOT_MASK;
    MemoEntry& entry = player.memo[slot];

    uint8_t slotDepth = entry.depth;
    if (slotDepth != 0 && slotDepth < depth) {
        int8_t score = entry.score;
        uint8_t flag = entry.flag;

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

    if (containsWin(getYellowPieces(board))) {
        return INT8_MAX - depth;
    }

    if (depth == player.maxDepth || isBoardFull(board)) {
        return 0;
    }

    int8_t score = INT8_MIN;
    for (uint8_t c = 0; c < 7; ++c) {
        if (isColumnFull(board, c)) continue;

        Board newBoard = board;
        placeYellowPiece(newBoard, c);
        int8_t newScore = -negaMaxRed(newBoard, player, depth + 1, -beta, -alpha);
        
        bool breakLoop = false;
        if (newScore > score) {
            score = newScore;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    breakLoop = true;
                }
            }
        }

        if (breakLoop) break;
    }

    insertMemoEntry(entry, score, player.turn + depth, originalAlpha, beta);
    return score;
}


uint8_t chooseColumn(const Player& player, const Board& board) {
    int8_t scores[7];
    for (uint8_t c = 0; c < 7; ++c) {
        if (isColumnFull(board, c)) {
            scores[c] = INT8_MIN;
        } else {
            Board newBoard = board;
            if (player.isRed) {
                placeRedPiece(newBoard, c);
                scores[c] = -negaMaxYellow(board, player, 1, INT8_MIN, INT8_MAX);
            } else {
                placeYellowPiece(newBoard, c);
                scores[c] = -negaMaxRed(board, player, 1, INT8_MIN, INT8_MAX);
            }
        }
    }

    uint8_t cols[7];
    uint8_t numCols = 0;
    int8_t highestScore = INT8_MIN;
    for (uint8_t c = 0; c < 7; ++c) {
        if (scores[c] > highestScore) {
            highestScore = scores[c];

            cols[0] = c;
            numCols = 1;
        } else if (scores[c] == highestScore) {
            cols[numCols] = c;
            ++numCols;
        }
    }

    if (numCols == 0) {
        return 0;
    } else if (numCols == 1) {
        return cols[0];
    }

    int idx = rand() % static_cast<int>(numCols);
    return cols[idx];
}

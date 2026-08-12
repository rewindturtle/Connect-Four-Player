#include "player.h"
#include <stdlib.h>


// Forward Declaration
int8_t negaMaxYellow(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta);


int8_t negaMaxRed(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta) {
    int8_t originalAlpha = alpha;

    uint64_t hash = hashBoard(board);
    uint32_t slot = hash & MEMO_SLOT_MASK;
    uint16_t key = (hash >> MEMO_BITS) & 0xFFFF;
    uint8_t horizon = player.turn + (player.maxDepth - depth);
    MemoEntry& entry = player.memo[slot];

    uint8_t slotHorizon = getMemoHorizon(entry);
    if (horizon < slotHorizon && entry.key == key) {
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

    // Yellow moved into this node, so a yellow win here is a loss for red
    if (containsWin(getYellowPieces(board))) {
        return -(MAX_SCORE - horizon);
    }

    if (depth == 0 || isBoardFull(board)) {
        return 0;
    }

    int8_t score = MIN_SCORE;
    for (uint8_t c = 0; c < 7; ++c) {
        if (isColumnFull(board, c)) continue;

        Board newBoard = board;
        placeRedPiece(newBoard, c);
        int8_t newScore = -negaMaxYellow(newBoard, player, depth - 1, -beta, -alpha);
        
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

    insertMemoEntry(entry, key, score, horizon, originalAlpha, beta);
    return score;
}


int8_t negaMaxYellow(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta) {
    int8_t originalAlpha = alpha;

    uint64_t hash = hashBoard(board);
    uint32_t slot = hash & MEMO_SLOT_MASK;
    uint16_t key = static_cast<uint16_t>(hash >> MEMO_BITS);
    uint8_t horizon = player.turn + (player.maxDepth - depth);
    MemoEntry& entry = player.memo[slot];

    uint8_t slotHorizon = getMemoHorizon(entry);
    if (horizon < slotHorizon && entry.key == key) {
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

    // Red moved into this node, so a red win here is a loss for yellow
    if (containsWin(board.redPieces)) {
        return -(MAX_SCORE - horizon);
    }

    if (depth == 0 || isBoardFull(board)) {
        return 0;
    }

    int8_t score = MIN_SCORE;
    for (uint8_t c = 0; c < 7; ++c) {
        if (isColumnFull(board, c)) continue;

        Board newBoard = board;
        placeYellowPiece(newBoard, c);
        int8_t newScore = -negaMaxRed(newBoard, player, depth - 1, -beta, -alpha);
        
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

    insertMemoEntry(entry, key, score, horizon, originalAlpha, beta);
    return score;
}


uint8_t chooseColumn(const Player& player, const Board& board) {
    int8_t scores[7];
    for (uint8_t c = 0; c < 7; ++c) {
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

    uint8_t cols[7];
    uint8_t numCols = 0;
    int8_t highestScore = MIN_SCORE;
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

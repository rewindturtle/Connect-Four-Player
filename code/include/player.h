#ifndef PLAYER_H
#define PLAYER_H

#include "board.h"
#include "memo.h"

// Forward Declaration
struct Player;

typedef int8_t (*ScoreFunc)(const Board& board, const Player& player, uint8_t depth);

inline int8_t standardscoreFunc(const Board& board, const Player& player, uint8_t depth) {
    return 0;
}


struct Player {
    MemoEntry* memo = nullptr;
    ScoreFunc scoreFunc = standardscoreFunc;
    uint8_t maxDepth = 4;
    uint8_t turn = 0;
    bool isRed = true;
};


uint8_t chooseColumn(const Player& player, const Board& board);


#endif // PLAYER_H
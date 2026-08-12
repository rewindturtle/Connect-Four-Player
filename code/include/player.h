#ifndef PLAYER_H
#define PLAYER_H

#include "board.h"
#include "memo.h"

#define STANDARD_PLAY_STYLE 0
#define MISTAKES_PLAY_STYLE 1
#define PROLONG_PLAY_STYLE 2


struct Player {
    MemoEntry* memo = nullptr;
    float mistakeProb = 0.2f;
    uint8_t maxDepth = 4;
    uint8_t turn = 0;
    uint8_t playStyle = STANDARD_PLAY_STYLE;
    bool isRed = true;
    volatile bool forceStop = false;
};


uint8_t chooseColumn(const Player& player, const Board& board);
void idleSearch(const Player& player, const Board& board);


#endif // PLAYER_H
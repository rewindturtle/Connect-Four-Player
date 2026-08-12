#ifndef PLAYER_H
#define PLAYER_H

#include "board.h"
#include "memo.h"


struct Player {
    MemoEntry* memo = nullptr;
    uint8_t maxDepth = 4;
    uint8_t turn = 0;
    bool isRed = true;
    volatile bool forceStop = false;
};


uint8_t chooseColumn(const Player& player, const Board& board);
void idleSearch(const Player& player, const Board& board);


#endif // PLAYER_H
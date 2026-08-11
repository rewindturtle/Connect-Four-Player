#ifndef PLAYER_H
#define PLAYER_H

#include "board.h"
#include "memo.h"


struct Player {
    MemoEntry* memo = nullptr;
    uint8_t maxDepth = 4;
    uint8_t turn = 0;
    bool isRed = true;
};


int8_t negaMaxRed(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta);
int8_t negaMaxYellow(const Board& board, const Player& player, uint8_t depth, int8_t alpha, int8_t beta);
uint8_t chooseColumn(const Player& player, const Board& board);


#endif // PLAYER_H
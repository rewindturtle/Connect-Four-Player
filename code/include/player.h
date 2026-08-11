#ifndef PLAYER_H
#define PLAYER_H

#include "board.h"
#include "memo.h"

// Forward Declaration
struct Player;

typedef int8_t (*EvalFn)(const Board& board, const Player& player);

struct Player {
    MemoEntry* memo = nullptr;
    EvalFn evalFunc = nullptr;
    uint8_t maxDepth = 4;
    uint8_t turn = 0;
    bool isRed = true;
};


inline int8_t standardEval(const Board& board, const Player& player) {
    return 0;
}


uint8_t chooseColumn(const Player& player, const Board& board);


#endif // PLAYER_H
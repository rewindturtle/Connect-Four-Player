#ifndef PLAYER_H
#define PLAYER_H

#include "board.h"
#include "memo.h"

#define STANDARD_PLAY_STYLE 0
#define MISTAKES_PLAY_STYLE 1
#define PROLONG_PLAY_STYLE 2
#define CENTER_PLAY_STYLE 3
#define EDGE_PLAY_STYLE 4
#define STACKER_PLAY_STYLE 5
#define SPREADER_PLAY_STYLE 6
#define PACIFIST_PLAY_STYLE 7
#define COPYCAT_PLAY_STYLE 8
#define TRAP_PLAY_STYLE 9

#define NO_COLUMN 0xFF


struct Player {
    MemoEntry* memo = nullptr;
    float mistakeProb = 0.2f;
    uint8_t maxDepth = 4;
    uint8_t panicDepth = 8;
    uint8_t turn = 0;
    uint8_t playStyle = STANDARD_PLAY_STYLE;
    uint8_t lastOpponentColumn = NO_COLUMN;
    bool isRed = true;
    bool canPanic = false;
    volatile bool forceStop = false;
};


uint8_t chooseColumn(const Player& player, const Board& board);
void idleSearch(const Player& player, const Board& board);


#endif // PLAYER_H
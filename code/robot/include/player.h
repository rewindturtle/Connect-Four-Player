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


// Describes the policy that actually selected a move. A play style can fall
// back to MOVE_REASON_BEST_SCORE when its preference cannot be applied.
enum MoveReason : uint8_t {
    MOVE_REASON_NO_LEGAL_MOVE,
    MOVE_REASON_BEST_SCORE,
    MOVE_REASON_INTENTIONAL_MISTAKE,
    MOVE_REASON_PROLONG_GAME,
    MOVE_REASON_PREFER_CENTER,
    MOVE_REASON_PREFER_EDGE,
    MOVE_REASON_STACK_HIGH,
    MOVE_REASON_SPREAD_LOW,
    MOVE_REASON_AVOID_WIN,
    MOVE_REASON_COPY_OPPONENT,
    MOVE_REASON_CREATE_TRAP,
};

// Scores are from the choosing player's perspective. Comparing score with
// bestScore reveals whether a style deliberately passed over the best move.
struct MoveDecision {
    uint8_t column = NO_COLUMN;
    int8_t score = MIN_SCORE;
    int8_t bestScore = MIN_SCORE;
    MoveReason reason = MOVE_REASON_NO_LEGAL_MOVE;
};

static_assert(sizeof(MoveDecision) == 4, "MoveDecision should remain byte-packed");


struct Player {
    Memo* memo = nullptr;
    float mistakeProb = 0.2f;
    uint8_t maxDepth = 4;
    uint8_t panicDepth = 8;
    uint8_t turn = 0;
    uint8_t playStyle = STANDARD_PLAY_STYLE;
    uint8_t lastOpponentColumn = NO_COLUMN;
    // Turn order and display colour are independent. Search only uses isFirst;
    // isRed is presentation/game configuration state.
    bool isFirst = true;
    bool isRed = true;
    bool canPanic = false;
    volatile bool forceStop = false;
};


MoveDecision chooseMove(const Player& player, const Board& board);
uint8_t chooseColumn(const Player& player, const Board& board);
void idleSearch(const Player& player, const Board& board);


#endif // PLAYER_H

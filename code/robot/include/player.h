#ifndef PLAYER_H
#define PLAYER_H

#include "board.h"
#include "memo.h"

#include <atomic>

#define NO_COLUMN 0xFF
#define INITIAL_SEARCH_DEPTH 3
#define DEFAULT_SEARCH_TIME_LIMIT_MS 5000
#define DEFAULT_PANIC_DEPTH 8


enum PlayStyle : uint8_t {
    STANDARD_PLAY_STYLE,
    MISTAKES_PLAY_STYLE,
    PROLONG_PLAY_STYLE,
    CENTER_PLAY_STYLE,
    EDGE_PLAY_STYLE,
    STACKER_PLAY_STYLE,
    SPREADER_PLAY_STYLE,
    PACIFIST_PLAY_STYLE,
    COPYCAT_PLAY_STYLE,
    TRAP_PLAY_STYLE,
    PLAY_STYLE_COUNT
};


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

class Player {
    private:
        Memo* _memo;
        float _mistakeProb;
        uint32_t _timeLimitMs;
        uint8_t _maxDepth;
        uint8_t _panicDepth;
        uint8_t _turn;
        PlayStyle _playStyle;
        uint8_t _lastOpponentColumn;
        // Turn order and display colour are independent. Search only uses
        // _isFirst; _isRed is presentation/game configuration state.
        bool _isFirst;
        bool _isRed;
        bool _canPanic;
        std::atomic<bool> _forceStop;
    public:
        explicit Player(Memo* memo = nullptr);

        MoveDecision chooseMove(const Board& board) const;
        uint8_t chooseColumn(const Board& board) const;
        void idleSearch(const Board& board) const;

        inline void setMemo(Memo* memo) {_memo = memo;}
        inline Memo* getMemo() const {return _memo;}
        inline void setMistakeProb(float probability) {_mistakeProb = probability;}
        inline float getMistakeProb() const {return _mistakeProb;}
        inline void setMaxDepth(uint8_t depth) {_maxDepth = depth;}
        inline uint8_t getMaxDepth() const {return _maxDepth;}
        inline void setPanicDepth(uint8_t depth) {_panicDepth = depth;}
        inline uint8_t getPanicDepth() const {return _panicDepth;}
        // A zero time limit disables the deadline for chooseMove().
        inline void setTimeLimitMs(uint32_t timeLimitMs) {_timeLimitMs = timeLimitMs;}
        inline uint32_t getTimeLimitMs() const {return _timeLimitMs;}
        inline void setTurn(uint8_t turn) {_turn = turn;}
        inline uint8_t getTurn() const {return _turn;}
        inline void setPlayStyle(PlayStyle playStyle) {_playStyle = playStyle;}
        inline PlayStyle getPlayStyle() const {return _playStyle;}
        inline void setLastOpponentColumn(uint8_t column) {_lastOpponentColumn = column;}
        inline uint8_t getLastOpponentColumn() const {return _lastOpponentColumn;}
        inline void setFirst(bool isFirst) {_isFirst = isFirst;}
        inline bool isFirst() const {return _isFirst;}
        inline void setRed(bool isRed) {_isRed = isRed;}
        inline bool isRed() const {return _isRed;}
        inline void setCanPanic(bool canPanic) {_canPanic = canPanic;}
        inline bool canPanic() const {return _canPanic;}
        inline void setForceStop(bool forceStop) {_forceStop.store(forceStop, std::memory_order_relaxed);}
        inline bool shouldStop() const {return _forceStop.load(std::memory_order_relaxed);}
};

#endif // PLAYER_H

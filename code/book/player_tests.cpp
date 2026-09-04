#include "board.h"
#include "memo.h"
#include "player.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>


static void configureShallowPlayer(Player& player, PlayStyle playStyle) {
    player.setMaxDepth(1);
    player.setPlayStyle(playStyle);
}


static Board makeImmediateWinBoard() {
    Board board = {0, 0};
    board.placeFirstPiece(0);
    board.placeSecondPiece(6);
    board.placeFirstPiece(1);
    board.placeSecondPiece(6);
    board.placeFirstPiece(2);
    board.placeSecondPiece(5);
    return board;
}


static Board makeFullBoard() {
    Board board = {0, 0};
    bool firstTurn = true;
    for (uint8_t row = 0; row < 6; ++row) {
        for (uint8_t col = 0; col < 7; ++col) {
            if (firstTurn) {
                board.placeFirstPiece(col);
            } else {
                board.placeSecondPiece(col);
            }
            firstTurn = !firstTurn;
        }
    }
    return board;
}


static void testBestScoreDecision() {
    Player player;
    configureShallowPlayer(player, STANDARD_PLAY_STYLE);
    Board board = {0, 0};

    MoveDecision decision = player.chooseMove(board);
    assert(decision.column < 7);
    assert(decision.score == 0);
    assert(decision.bestScore == 0);
    assert(decision.reason == MOVE_REASON_BEST_SCORE);
    assert(player.chooseColumn(board) < 7);
}


static void testStyleReasons() {
    Board empty = {0, 0};

    Player center;
    configureShallowPlayer(center, CENTER_PLAY_STYLE);
    MoveDecision centerDecision = center.chooseMove(empty);
    assert(centerDecision.column == 3);
    assert(centerDecision.reason == MOVE_REASON_PREFER_CENTER);

    Player edge;
    configureShallowPlayer(edge, EDGE_PLAY_STYLE);
    MoveDecision edgeDecision = edge.chooseMove(empty);
    assert(edgeDecision.column == 6);
    assert(edgeDecision.reason == MOVE_REASON_PREFER_EDGE);

    Player copycat;
    configureShallowPlayer(copycat, COPYCAT_PLAY_STYLE);
    copycat.setLastOpponentColumn(2);
    MoveDecision copyDecision = copycat.chooseMove(empty);
    assert(copyDecision.column == 2);
    assert(copyDecision.reason == MOVE_REASON_COPY_OPPONENT);
}


static void testDeliberatelySuboptimalDecisions() {
    Board board = makeImmediateWinBoard();

    Player mistakes;
    configureShallowPlayer(mistakes, MISTAKES_PLAY_STYLE);
    mistakes.setMistakeProb(2.0f);
    MoveDecision mistake = mistakes.chooseMove(board);
    assert(mistake.reason == MOVE_REASON_INTENTIONAL_MISTAKE);
    assert(mistake.score == 0);
    assert(mistake.bestScore > 0);
    assert(mistake.score < mistake.bestScore);

    Player pacifist;
    configureShallowPlayer(pacifist, PACIFIST_PLAY_STYLE);
    MoveDecision avoidedWin = pacifist.chooseMove(board);
    assert(avoidedWin.reason == MOVE_REASON_AVOID_WIN);
    assert(avoidedWin.score == 0);
    assert(avoidedWin.bestScore > 0);
    assert(avoidedWin.score < avoidedWin.bestScore);

    Player copycat;
    configureShallowPlayer(copycat, COPYCAT_PLAY_STYLE);
    copycat.setLastOpponentColumn(4);
    MoveDecision refusedCopy = copycat.chooseMove(board);
    assert(refusedCopy.column == 3);
    assert(refusedCopy.score == refusedCopy.bestScore);
    assert(refusedCopy.reason == MOVE_REASON_BEST_SCORE);
}


static void testNoLegalMove() {
    Player player;
    configureShallowPlayer(player, STANDARD_PLAY_STYLE);
    Board board = makeFullBoard();

    MoveDecision decision = player.chooseMove(board);
    assert(decision.column == NO_COLUMN);
    assert(decision.score == MIN_SCORE);
    assert(decision.bestScore == MIN_SCORE);
    assert(decision.reason == MOVE_REASON_NO_LEGAL_MOVE);
    assert(player.chooseColumn(board) == NO_COLUMN);
}


static void testMemoBackedSearch() {
    Memo memo;
    assert(memo.isValid());

    Player player(&memo);
    player.setMaxDepth(2);
    MoveDecision decision = player.chooseMove(Board{0, 0});

    assert(decision.column < 7);
    assert(decision.score == 0);
}


static void testIterativeDeepeningReachesMaxDepth() {
    Memo memo;
    assert(memo.isValid());

    Player player(&memo);
    player.setMaxDepth(4);
    player.setTimeLimitMs(0);

    Board board;
    MoveDecision decision = player.chooseMove(board);
    assert(decision.column < 7);

    // Column 6 is searched last. Its child entry can only reach depth 3 after
    // every root column has completed at root depth 4.
    Board lastChild = board;
    lastChild.placeFirstPiece(6);
    uint64_t key = lastChild.getKey();
    uint32_t slot = key & MEMO_SLOT_MASK;
    uint32_t tag = static_cast<uint32_t>(key >> MEMO_BITS);
    assert(memo.getTag(slot) == tag);
    assert(memo.getDepth(slot) == 3);
}


static void testPanicExtendsThreatenedSearch() {
    Memo memo;
    assert(memo.isValid());

    Player player(&memo);
    assert(!player.canPanic());
    assert(player.getPanicDepth() == DEFAULT_PANIC_DEPTH);
    player.setCanPanic(true);
    player.setMaxDepth(2);
    player.setPanicDepth(3);
    player.setTimeLimitMs(0);

    // Second threatens a vertical win in column 6. The non-blocking root moves
    // score negatively at depth 2 and trigger the extra panic depth.
    Board board;
    board.placeFirstPiece(0);
    board.placeSecondPiece(6);
    board.placeFirstPiece(1);
    board.placeSecondPiece(6);
    board.placeFirstPiece(0);
    board.placeSecondPiece(6);

    MoveDecision decision = player.chooseMove(board);
    assert(decision.column == 6);

    // The neutral blocking move in column 6 is searched last, so reaching
    // depth 2 in its child proves the root iteration at panic depth 3 finished.
    Board lastChild = board;
    lastChild.placeFirstPiece(6);
    uint64_t key = lastChild.getKey();
    uint32_t slot = key & MEMO_SLOT_MASK;
    uint32_t tag = static_cast<uint32_t>(key >> MEMO_BITS);
    assert(memo.getTag(slot) == tag);
    assert(memo.getDepth(slot) == 2);
}


static void testIdleSearchIgnoresTimeLimitAndHonoursForceStop() {
    Memo memo;
    assert(memo.isValid());

    Player player(&memo);
    player.setMaxDepth(4);
    player.setTimeLimitMs(1);

    Board board;
    player.idleSearch(board);

    uint64_t key = board.getKey();
    uint32_t slot = key & MEMO_SLOT_MASK;
    uint32_t tag = static_cast<uint32_t>(key >> MEMO_BITS);
    assert(memo.getTag(slot) == tag);
    assert(memo.getDepth(slot) == 4);

    memo.reset();
    player.setForceStop(true);
    player.idleSearch(board);
    assert(!memo.isSlotInitialized(slot));
}


int main() {
    testBestScoreDecision();
    testStyleReasons();
    testDeliberatelySuboptimalDecisions();
    testNoLegalMove();
    testMemoBackedSearch();
    testIterativeDeepeningReachesMaxDepth();
    testPanicExtendsThreatenedSearch();
    testIdleSearchIgnoresTimeLimitAndHonoursForceStop();
    std::puts("player tests passed");
    return 0;
}

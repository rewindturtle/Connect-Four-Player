#include "board.h"
#include "memo.h"
#include "player.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>


static Player makeShallowPlayer(PlayStyle playStyle) {
    Player player;
    player.setMaxDepth(1);
    player.setPlayStyle(playStyle);
    return player;
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
    Player player = makeShallowPlayer(STANDARD_PLAY_STYLE);
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

    Player center = makeShallowPlayer(CENTER_PLAY_STYLE);
    MoveDecision centerDecision = center.chooseMove(empty);
    assert(centerDecision.column == 3);
    assert(centerDecision.reason == MOVE_REASON_PREFER_CENTER);

    Player edge = makeShallowPlayer(EDGE_PLAY_STYLE);
    MoveDecision edgeDecision = edge.chooseMove(empty);
    assert(edgeDecision.column == 6);
    assert(edgeDecision.reason == MOVE_REASON_PREFER_EDGE);

    Player copycat = makeShallowPlayer(COPYCAT_PLAY_STYLE);
    copycat.setLastOpponentColumn(2);
    MoveDecision copyDecision = copycat.chooseMove(empty);
    assert(copyDecision.column == 2);
    assert(copyDecision.reason == MOVE_REASON_COPY_OPPONENT);
}


static void testDeliberatelySuboptimalDecisions() {
    Board board = makeImmediateWinBoard();

    Player mistakes = makeShallowPlayer(MISTAKES_PLAY_STYLE);
    mistakes.setMistakeProb(2.0f);
    MoveDecision mistake = mistakes.chooseMove(board);
    assert(mistake.reason == MOVE_REASON_INTENTIONAL_MISTAKE);
    assert(mistake.score == 0);
    assert(mistake.bestScore > 0);
    assert(mistake.score < mistake.bestScore);

    Player pacifist = makeShallowPlayer(PACIFIST_PLAY_STYLE);
    MoveDecision avoidedWin = pacifist.chooseMove(board);
    assert(avoidedWin.reason == MOVE_REASON_AVOID_WIN);
    assert(avoidedWin.score == 0);
    assert(avoidedWin.bestScore > 0);
    assert(avoidedWin.score < avoidedWin.bestScore);

    Player copycat = makeShallowPlayer(COPYCAT_PLAY_STYLE);
    copycat.setLastOpponentColumn(4);
    MoveDecision refusedCopy = copycat.chooseMove(board);
    assert(refusedCopy.column == 3);
    assert(refusedCopy.score == refusedCopy.bestScore);
    assert(refusedCopy.reason == MOVE_REASON_BEST_SCORE);
}


static void testNoLegalMove() {
    Player player = makeShallowPlayer(STANDARD_PLAY_STYLE);
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


int main() {
    testBestScoreDecision();
    testStyleReasons();
    testDeliberatelySuboptimalDecisions();
    testNoLegalMove();
    testMemoBackedSearch();
    std::puts("player tests passed");
    return 0;
}

#include "board.h"
#include "memo.h"
#include "player.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>


static Player makeShallowPlayer(uint8_t playStyle) {
    Player player;
    player.maxDepth = 1;
    player.playStyle = playStyle;
    return player;
}


static Board makeImmediateWinBoard() {
    Board board = {0, 0};
    placeFirstPiece(board, 0);
    placeSecondPiece(board, 6);
    placeFirstPiece(board, 1);
    placeSecondPiece(board, 6);
    placeFirstPiece(board, 2);
    placeSecondPiece(board, 5);
    return board;
}


static Board makeFullBoard() {
    Board board = {0, 0};
    bool firstTurn = true;
    for (uint8_t row = 0; row < 6; ++row) {
        for (uint8_t col = 0; col < 7; ++col) {
            if (firstTurn) {
                placeFirstPiece(board, col);
            } else {
                placeSecondPiece(board, col);
            }
            firstTurn = !firstTurn;
        }
    }
    return board;
}


static void testBestScoreDecision() {
    Player player = makeShallowPlayer(STANDARD_PLAY_STYLE);
    Board board = {0, 0};

    MoveDecision decision = chooseMove(player, board);
    assert(decision.column < 7);
    assert(decision.score == 0);
    assert(decision.bestScore == 0);
    assert(decision.reason == MOVE_REASON_BEST_SCORE);
    assert(chooseColumn(player, board) < 7);
}


static void testStyleReasons() {
    Board empty = {0, 0};

    Player center = makeShallowPlayer(CENTER_PLAY_STYLE);
    MoveDecision centerDecision = chooseMove(center, empty);
    assert(centerDecision.column == 3);
    assert(centerDecision.reason == MOVE_REASON_PREFER_CENTER);

    Player edge = makeShallowPlayer(EDGE_PLAY_STYLE);
    MoveDecision edgeDecision = chooseMove(edge, empty);
    assert(edgeDecision.column == 6);
    assert(edgeDecision.reason == MOVE_REASON_PREFER_EDGE);

    Player copycat = makeShallowPlayer(COPYCAT_PLAY_STYLE);
    copycat.lastOpponentColumn = 2;
    MoveDecision copyDecision = chooseMove(copycat, empty);
    assert(copyDecision.column == 2);
    assert(copyDecision.reason == MOVE_REASON_COPY_OPPONENT);
}


static void testDeliberatelySuboptimalDecisions() {
    Board board = makeImmediateWinBoard();

    Player mistakes = makeShallowPlayer(MISTAKES_PLAY_STYLE);
    mistakes.mistakeProb = 2.0f;
    MoveDecision mistake = chooseMove(mistakes, board);
    assert(mistake.reason == MOVE_REASON_INTENTIONAL_MISTAKE);
    assert(mistake.score == 0);
    assert(mistake.bestScore > 0);
    assert(mistake.score < mistake.bestScore);

    Player pacifist = makeShallowPlayer(PACIFIST_PLAY_STYLE);
    MoveDecision avoidedWin = chooseMove(pacifist, board);
    assert(avoidedWin.reason == MOVE_REASON_AVOID_WIN);
    assert(avoidedWin.score == 0);
    assert(avoidedWin.bestScore > 0);
    assert(avoidedWin.score < avoidedWin.bestScore);

    Player copycat = makeShallowPlayer(COPYCAT_PLAY_STYLE);
    copycat.lastOpponentColumn = 4;
    MoveDecision refusedCopy = chooseMove(copycat, board);
    assert(refusedCopy.column == 3);
    assert(refusedCopy.score == refusedCopy.bestScore);
    assert(refusedCopy.reason == MOVE_REASON_BEST_SCORE);
}


static void testNoLegalMove() {
    Player player = makeShallowPlayer(STANDARD_PLAY_STYLE);
    Board board = makeFullBoard();

    MoveDecision decision = chooseMove(player, board);
    assert(decision.column == NO_COLUMN);
    assert(decision.score == MIN_SCORE);
    assert(decision.bestScore == MIN_SCORE);
    assert(decision.reason == MOVE_REASON_NO_LEGAL_MOVE);
    assert(chooseColumn(player, board) == NO_COLUMN);
}


int main() {
    testBestScoreDecision();
    testStyleReasons();
    testDeliberatelySuboptimalDecisions();
    testNoLegalMove();
    std::puts("player tests passed");
    return 0;
}

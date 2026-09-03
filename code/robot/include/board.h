#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

// Column-major bitboard: a cell is bit (7 * col + row), row 0 at the bottom.
// Each column gets 7 bits for 6 cells; the spare row-6 bit is a sentinel that
// is never filled, so a four-in-a-row shift can never wrap into the next
// column. That is what lets containsWin be four shift-and pairs.
#define COLUMN_MASK 0x3FULL
#define BOARD_MASK 0x0000FDFBF7EFDFBFULL
#define BOTTOM_MASK 0x0000040810204081ULL
#define BOARD_KEY_MASK 0x0001FFFFFFFFFFFFULL


class Board {
    private:
        // Pieces are stored by turn order, not display colour. This keeps
        // search and memoization independent of whether the first player is
        // red/yellow or controlled by the robot/human.
        uint64_t _firstPieces;
        uint64_t _allPieces;

        uint64_t _landingMask(uint8_t col) const;

    public:
        Board(uint64_t firstPieces = 0, uint64_t allPieces = 0);

        inline uint64_t getFirstPieces() const {return _firstPieces;}
        inline uint64_t getSecondPieces() const {return _firstPieces ^ _allPieces;}
        inline uint64_t getAllPieces() const {return _allPieces;}
        inline uint8_t getOccupiedCount() const {return static_cast<uint8_t>(__builtin_popcountll(_allPieces));}

        inline uint64_t getRedPieces(bool firstIsRed) const {return firstIsRed ? _firstPieces : getSecondPieces();}
        inline uint64_t getYellowPieces(bool firstIsRed) const {return firstIsRed ? getSecondPieces() : _firstPieces;}

        static inline bool isPieceRowCol(uint64_t pieces, uint8_t row, uint8_t col) {
            return (pieces & (1ULL << (7 * col + row))) != 0;
        }

        static inline bool isPiecePos(uint64_t pieces, uint8_t pos) {
            return (pieces & (1ULL << pos)) != 0;
        }

        static inline bool isPieceMask(uint64_t pieces, uint64_t mask) {
            return (pieces & mask) != 0;
        }

        inline bool isColumnFull(uint8_t col) const {
            return (_allPieces & (1ULL << (7 * col + 5))) != 0;
        }

        inline uint8_t getColumnHeight(uint8_t col) const {
            return static_cast<uint8_t>(__builtin_popcountll(_allPieces & (COLUMN_MASK << (7 * col))));
        }

        inline bool isFull() const {return (_allPieces & BOARD_MASK) == BOARD_MASK;}
        inline bool firstHasWin() const {return containsWin(_firstPieces);}
        inline bool secondHasWin() const {return containsWin(getSecondPieces());}
        inline bool hasWin() const {return firstHasWin() || secondHasWin();}

        static bool containsWin(uint64_t pieces);
        void placeFirstPiece(uint8_t col);
        void placeSecondPiece(uint8_t col);

        // A collision-free 49-bit encoding of every gravity-valid board,
        // reversibly mixed for an even memo-table distribution. The first
        // player is represented directly; display colour is not part of it.
        uint64_t getKey() const;
};

static_assert(sizeof(Board) == 16, "Board must remain two bitboards");


#endif // BOARD_H

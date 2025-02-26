#ifndef MOVEGEN_H_INCLUDED
#define MOVEGEN_H_INCLUDED

#include <iostream>
#include <unordered_map>
#include <string>
#include <sstream>

#include "position.h"
#include "piece.h"

#define encodeMove(source, target, piece, promoted, capture, double, enpassant, castling) \
(source) | (target << 6)| (piece << 12) | (promoted << 16) | (capture << 20) | (double << 21) | (enpassant << 22) | (castling << 23)

// binary move bits
    /*
    0000 0000 0000 0000 0011 1111   source square   0x3f
    0000 0000 0000 1111 1100 0000   target square 0xfc0
    0000 0000 1111 0000 0000 0000   piece   0xf000
    0000 1111 0000 0000 0000 0000   promoted piece  0xf0000
    0001 0000 0000 0000 0000 0000   capture flag    0x100000
    0010 0000 0000 0000 0000 0000   double push flag 0x200000
    0100 0000 0000 0000 0000 0000   enpassant flag 0x400000
    1000 0000 0000 0000 0000 0000   castling flag 0x800000
    */

// 0x3f source square constant
#define getMoveSource(move) (move & 0x3f)

#define getMoveTarget(move) ((move & 0xfc0) >> 6)

#define getMovePiece(move) ((move & 0xf000) >> 12)

#define getMovePromotion(move) ((move & 0xf0000) >> 16)

#define getMoveCapture(move) (move & 0xf100000)

#define getDoublePush(move) (move & 0x200000)

#define getMoveEnpassant(move) (move & 0x400000)

#define getMoveCastling(move) (move & 0x800000)

namespace Sloth {
	namespace Movegen {
        // MIGHT MOVE MOVELIST
        typedef struct {
            int moves[256];
            int count;
        } MoveList;

        static  void addMove(MoveList* moveList, int move) {
            moveList->moves[moveList->count] = move;
            moveList->count++;
        }

        extern std::unordered_map<Piece::Pieces, char> promotedPieces;

        extern  void printMove(int move);
        extern  std::string moveToString(int move);
        extern  void printMoveList(MoveList* moveList);

		//extern  void generateMoves(Position &pos, MoveList* moveCount);
        extern  void generateMoves(Position& pos, MoveList* moveList, bool captures);
	}
}

#endif
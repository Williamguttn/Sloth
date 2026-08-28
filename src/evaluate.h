#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include "position.h"

namespace Sloth {

    namespace Eval {
        enum PieceTypes { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NB_PIECE };

        extern int evaluate(Position& pos);
    }
}

#endif
#include <iostream>
#include <unordered_map>
#include <string>
#include <sstream>

#include "movegen.h"
#include "bitboards.h"
#include "magic.h"
#include "piece.h"
#include "position.h"
#include "types.h"

namespace Sloth {

    const char* squareToCoordinates[] = {
        "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
        "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
        "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
        "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
        "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
        "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
        "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
        "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    };

    std::unordered_map<Piece::Pieces, char> Movegen::promotedPieces = {
        {Piece::Q, 'q'}, {Piece::R, 'r'}, {Piece::B, 'b'}, {Piece::N, 'n'},
        {Piece::q, 'q'}, {Piece::r, 'r'}, {Piece::b, 'b'}, {Piece::n, 'n'}
    };

    void Movegen::printMove(int move) {
        if (getMovePromotion(move)) {
            Piece::Pieces promotion = static_cast<Piece::Pieces>(getMovePromotion(move));
            printf("%s%s%c", squareToCoordinates[getMoveSource(move)], squareToCoordinates[getMoveTarget(move)], promotedPieces[promotion]);
        } else {
            printf("%s%s", squareToCoordinates[getMoveSource(move)], squareToCoordinates[getMoveTarget(move)]);
        }
    }

    std::string Movegen::moveToString(int move) {
        std::stringstream ss;
        if (getMovePromotion(move)) {
            Piece::Pieces promotion = static_cast<Piece::Pieces>(getMovePromotion(move));
            ss << squareToCoordinates[getMoveSource(move)]
               << squareToCoordinates[getMoveTarget(move)]
               << promotedPieces[promotion];
        } else {
            ss << squareToCoordinates[getMoveSource(move)]
               << squareToCoordinates[getMoveTarget(move)];
        }
        return ss.str();
    }

    void Movegen::printMoveList(Movegen::MoveList* moveList) {
        if (!moveList->count) {
            printf("\n    No moves in the move list");
            return;
        }
        printf("\nmove  piece capture double enpassant castling\n\n");
        for (int c = 0; c < moveList->count; c++) {
            int move = moveList->moves[c];
            Piece::Pieces promotion = static_cast<Piece::Pieces>(getMovePromotion(move));
            printf("%s%s%c %c     %d       %d      %d        %d\n", 
                   squareToCoordinates[getMoveSource(move)], 
                   squareToCoordinates[getMoveTarget(move)], 
                   getMovePromotion(move) ? promotedPieces[promotion] : ' ',
                   Piece::asciiPieces[getMovePiece(move)], 
                   getMoveCapture(move) ? 1 : 0, 
                   getDoublePush(move) ? 1 : 0, 
                   getMoveEnpassant(move) ? 1 : 0, 
                   getMoveCastling(move) ? 1 : 0);
        }
        printf("\n\nNumber of moves: %d\n\n", moveList->count);
    }

    // Helper function to add promotion moves
    static void addPromotionMoves(Movegen::MoveList* moveList, int source, int target, int piece, int capture) {
        int promotionPieces[4] = {
            piece == Piece::P ? Piece::Q : Piece::q,
            piece == Piece::P ? Piece::R : Piece::r,
            piece == Piece::P ? Piece::B : Piece::b,
            piece == Piece::P ? Piece::N : Piece::n
        };
        for (int i = 0; i < 4; i++) {
            Movegen::addMove(moveList, encodeMove(source, target, piece, promotionPieces[i], capture, 0, 0, 0));
        }
    }

    // Generate pawn moves
    static void generatePawnMoves(Position& pos, Movegen::MoveList* moveList, bool captures) {
        int side = pos.sideToMove;
        int pawnPiece = (side == Colors::white) ? Piece::P : Piece::p;
        U64 bb = pos.bitboards[pawnPiece];
        int direction = (side == Colors::white) ? -8 : 8;
        int promotionRankStart = (side == Colors::white) ? 0 : 56;
        int promotionRankEnd = (side == Colors::white) ? 7 : 63;
        int doublePushStart = (side == Colors::white) ? a2 : a7;
        int doublePushEnd = (side == Colors::white) ? h2 : h7;

        while (bb) {
            int source = Bitboards::getLs1bIndex(bb);
            int target = source + direction;

            // Quiet moves and promotions
            if (target >= a8 && target <= h1 && !getBit(pos.occupancies[Colors::both], target)) {
                bool isPromotion = (target >= promotionRankStart && target <= promotionRankEnd);
                if (isPromotion) {
                    addPromotionMoves(moveList, source, target, pawnPiece, 0);
                } else if (!captures) {
                    Movegen::addMove(moveList, encodeMove(source, target, pawnPiece, 0, 0, 0, 0, 0));
                    if (source >= doublePushStart && source <= doublePushEnd) {
                        int doubleTarget = target + direction;
                        if (!getBit(pos.occupancies[Colors::both], doubleTarget)) {
                            Movegen::addMove(moveList, encodeMove(source, doubleTarget, pawnPiece, 0, 0, 1, 0, 0));
                        }
                    }
                }
            }

            // Captures
            U64 pawnAttacks = Bitboards::pawnAttacks[side][source];
            U64 captureAttacks = pawnAttacks & pos.occupancies[1 - side];
            while (captureAttacks) {
                target = Bitboards::getLs1bIndex(captureAttacks);
                bool isPromotion = (target >= promotionRankStart && target <= promotionRankEnd);
                if (isPromotion) {
                    addPromotionMoves(moveList, source, target, pawnPiece, 1);
                } else {
                    Movegen::addMove(moveList, encodeMove(source, target, pawnPiece, 0, 1, 0, 0, 0));
                }
                popBit(captureAttacks, target);
            }

            // En passant
            if (pos.enPassant != no_sq) {
                U64 epAttacks = pawnAttacks & (1ULL << pos.enPassant);
                if (epAttacks) {
                    int epTarget = Bitboards::getLs1bIndex(epAttacks);
                    Movegen::addMove(moveList, encodeMove(source, epTarget, pawnPiece, 0, 1, 0, 1, 0));
                }
            }

            popBit(bb, source);
        }
    }

    // Generate knight moves
    static void generateKnightMoves(Position& pos, Movegen::MoveList* moveList, bool captures) {
        int side = pos.sideToMove;
        int knightPiece = (side == Colors::white) ? Piece::N : Piece::n;
        U64 bb = pos.bitboards[knightPiece];

        while (bb) {
            int source = Bitboards::getLs1bIndex(bb);
            U64 attacks = Bitboards::knightAttacks[source] & ~pos.occupancies[side];
            while (attacks) {
                int target = Bitboards::getLs1bIndex(attacks);
                if (getBit(pos.occupancies[1 - side], target)) {
                    Movegen::addMove(moveList, encodeMove(source, target, knightPiece, 0, 1, 0, 0, 0));
                } else if (!captures) {
                    Movegen::addMove(moveList, encodeMove(source, target, knightPiece, 0, 0, 0, 0, 0));
                }
                popBit(attacks, target);
            }
            popBit(bb, source);
        }
    }

    // Generate bishop moves
    static void generateBishopMoves(Position& pos, Movegen::MoveList* moveList, bool captures) {
        int side = pos.sideToMove;
        int bishopPiece = (side == Colors::white) ? Piece::B : Piece::b;
        U64 bb = pos.bitboards[bishopPiece];

        while (bb) {
            int source = Bitboards::getLs1bIndex(bb);
            U64 attacks = Magic::getBishopAttacks(source, pos.occupancies[Colors::both]) & ~pos.occupancies[side];
            while (attacks) {
                int target = Bitboards::getLs1bIndex(attacks);
                if (getBit(pos.occupancies[1 - side], target)) {
                    Movegen::addMove(moveList, encodeMove(source, target, bishopPiece, 0, 1, 0, 0, 0));
                } else if (!captures) {
                    Movegen::addMove(moveList, encodeMove(source, target, bishopPiece, 0, 0, 0, 0, 0));
                }
                popBit(attacks, target);
            }
            popBit(bb, source);
        }
    }

    // Generate rook moves
    static void generateRookMoves(Position& pos, Movegen::MoveList* moveList, bool captures) {
        int side = pos.sideToMove;
        int rookPiece = (side == Colors::white) ? Piece::R : Piece::r;
        U64 bb = pos.bitboards[rookPiece];

        while (bb) {
            int source = Bitboards::getLs1bIndex(bb);
            U64 attacks = Magic::getRookAttacks(source, pos.occupancies[Colors::both]) & ~pos.occupancies[side];
            while (attacks) {
                int target = Bitboards::getLs1bIndex(attacks);
                if (getBit(pos.occupancies[1 - side], target)) {
                    Movegen::addMove(moveList, encodeMove(source, target, rookPiece, 0, 1, 0, 0, 0));
                } else if (!captures) {
                    Movegen::addMove(moveList, encodeMove(source, target, rookPiece, 0, 0, 0, 0, 0));
                }
                popBit(attacks, target);
            }
            popBit(bb, source);
        }
    }

    // Generate queen moves
    static void generateQueenMoves(Position& pos, Movegen::MoveList* moveList, bool captures) {
        int side = pos.sideToMove;
        int queenPiece = (side == Colors::white) ? Piece::Q : Piece::q;
        U64 bb = pos.bitboards[queenPiece];

        while (bb) {
            int source = Bitboards::getLs1bIndex(bb);
            U64 attacks = Magic::getQueenAttacks(source, pos.occupancies[Colors::both]) & ~pos.occupancies[side];
            while (attacks) {
                int target = Bitboards::getLs1bIndex(attacks);
                if (getBit(pos.occupancies[1 - side], target)) {
                    Movegen::addMove(moveList, encodeMove(source, target, queenPiece, 0, 1, 0, 0, 0));
                } else if (!captures) {
                    Movegen::addMove(moveList, encodeMove(source, target, queenPiece, 0, 0, 0, 0, 0));
                }
                popBit(attacks, target);
            }
            popBit(bb, source);
        }
    }

    // Generate king moves
    static void generateKingMoves(Position& pos, Movegen::MoveList* moveList, bool captures) {
        int side = pos.sideToMove;
        int kingPiece = (side == Colors::white) ? Piece::K : Piece::k;
        U64 bb = pos.bitboards[kingPiece];

        while (bb) {
            int source = Bitboards::getLs1bIndex(bb);
            U64 attacks = Bitboards::kingAttacks[source] & ~pos.occupancies[side];
            while (attacks) {
                int target = Bitboards::getLs1bIndex(attacks);
                if (getBit(pos.occupancies[1 - side], target)) {
                    Movegen::addMove(moveList, encodeMove(source, target, kingPiece, 0, 1, 0, 0, 0));
                } else if (!captures) {
                    Movegen::addMove(moveList, encodeMove(source, target, kingPiece, 0, 0, 0, 0, 0));
                }
                popBit(attacks, target);
            }
            popBit(bb, source);
        }
    }

    // Generate castling moves
    static void generateCastlingMoves(Position& pos, Movegen::MoveList* moveList) {
        int side = pos.sideToMove;
        int kingPiece = (side == Colors::white) ? Piece::K : Piece::k;
        int enemySide = 1 - side;

        if (side == Colors::white) {
            if (pos.castle & CastlingRights::WK) {
                if (!getBit(pos.occupancies[Colors::both], f1) && !getBit(pos.occupancies[Colors::both], g1)) {
                    if (!pos.isSquareAttacked(e1, enemySide) && !pos.isSquareAttacked(f1, enemySide)) {
                        Movegen::addMove(moveList, encodeMove(e1, g1, kingPiece, 0, 0, 0, 0, 1));
                    }
                }
            }
            if (pos.castle & CastlingRights::WQ) {
                if (!getBit(pos.occupancies[Colors::both], d1) && 
                    !getBit(pos.occupancies[Colors::both], c1) && 
                    !getBit(pos.occupancies[Colors::both], b1)) {
                    if (!pos.isSquareAttacked(e1, enemySide) && !pos.isSquareAttacked(d1, enemySide)) {
                        Movegen::addMove(moveList, encodeMove(e1, c1, kingPiece, 0, 0, 0, 0, 1));
                    }
                }
            }
        } else {
            if (pos.castle & CastlingRights::BK) {
                if (!getBit(pos.occupancies[Colors::both], f8) && !getBit(pos.occupancies[Colors::both], g8)) {
                    if (!pos.isSquareAttacked(e8, enemySide) && !pos.isSquareAttacked(f8, enemySide)) {
                        Movegen::addMove(moveList, encodeMove(e8, g8, kingPiece, 0, 0, 0, 0, 1));
                    }
                }
            }
            if (pos.castle & CastlingRights::BQ) {
                if (!getBit(pos.occupancies[Colors::both], d8) && 
                    !getBit(pos.occupancies[Colors::both], c8) && 
                    !getBit(pos.occupancies[Colors::both], b8)) {
                    if (!pos.isSquareAttacked(e8, enemySide) && !pos.isSquareAttacked(d8, enemySide)) {
                        Movegen::addMove(moveList, encodeMove(e8, c8, kingPiece, 0, 0, 0, 0, 1));
                    }
                }
            }
        }
    }

    // Main move generation function
    void Movegen::generateMoves(Position& pos, Movegen::MoveList* moveList, bool captures) {
        moveList->count = 0;

        generatePawnMoves(pos, moveList, captures);
        generateKnightMoves(pos, moveList, captures);
        generateBishopMoves(pos, moveList, captures);
        generateRookMoves(pos, moveList, captures);
        generateQueenMoves(pos, moveList, captures);
        generateKingMoves(pos, moveList, captures);
        
        if (!captures) {
            generateCastlingMoves(pos, moveList);
        }
    }
}
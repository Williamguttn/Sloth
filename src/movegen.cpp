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
		{Piece::Q, 'q'},
		{Piece::R, 'r'},
		{Piece::B, 'b'},
		{Piece::N, 'n'},
		{Piece::q, 'q'},
		{Piece::r, 'r'},
		{Piece::b, 'b'},
		{Piece::n, 'n'}
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

	void Movegen::printMoveList(MoveList* moveList) {
		if (!moveList->count) {
			printf("\n	No moves in the move list");
			return;
		}
		printf("\nmove  piece capture double enpassant castling\n\n");
		for (int c = 0; c < moveList->count; c++) {
			int move = moveList->moves[c];
			Piece::Pieces promotion = static_cast<Piece::Pieces>(getMovePromotion(move));
			printf("%s%s%c %c     %d	     %d  	%d	   %d\n", squareToCoordinates[getMoveSource(move)], squareToCoordinates[getMoveTarget(move)], getMovePromotion(move) ? promotedPieces[promotion] : ' ',
				Piece::asciiPieces[getMovePiece(move)], getMoveCapture(move) ? 1 : 0, getDoublePush(move) ? 1 : 0, getMoveEnpassant(move) ? 1 : 0, getMoveCastling(move) ? 1 : 0);
		}
		printf("\n\nNumber of moves: %d\n\n", moveList->count);
	}

	void Movegen::generateMoves(Position& pos, MoveList* moveList, bool captures) {
		moveList->count = 0;
		int sourceSquare, target;
		U64 bb, attacks;

		for (int piece = Piece::P; piece <= Piece::k; piece++) {
			bb = Bitboards::bitboards[piece];
			if (pos.sideToMove == Colors::white) {
				if (piece == Piece::P) {
					while (bb) {
						sourceSquare = Bitboards::getLs1bIndex(bb);
						target = sourceSquare - 8;
						if (!(target < a8) && !getBit(Bitboards::occupancies[both], target)) {
							if (captures && sourceSquare >= a7 && sourceSquare <= h7) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::Q, 0, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::R, 0, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::B, 0, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::N, 0, 0, 0, 0));
							} else {
								if (sourceSquare >= a7 && sourceSquare <= h7) {
									addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::Q, 0, 0, 0, 0));
									addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::R, 0, 0, 0, 0));
									addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::B, 0, 0, 0, 0));
									addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::N, 0, 0, 0, 0));
								} else {
									addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 0, 0, 0, 0));
									if ((sourceSquare >= a2 && sourceSquare <= h2) && !getBit(Bitboards::occupancies[both], (target - 8))) {
										addMove(moveList, encodeMove(sourceSquare, (target - 8), piece, 0, 0, 1, 0, 0));
									}
								}
							}
						}
						U64 pawnLookup = Bitboards::pawnAttacks[pos.sideToMove][sourceSquare];
						attacks = pawnLookup & Bitboards::occupancies[Colors::black];
						while (attacks) {
							target = Bitboards::getLs1bIndex(attacks);
							if (sourceSquare >= a7 && sourceSquare <= h7) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::Q, 1, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::R, 1, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::B, 1, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::N, 1, 0, 0, 0));
							} else {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
							popBit(attacks, target);
						}
						if (pos.enPassant != no_sq) {
							U64 enPassantAttacks = Bitboards::pawnAttacks[pos.sideToMove][sourceSquare] & (1ULL << pos.enPassant);
							if (enPassantAttacks) {
								int targetEnPassant = Bitboards::getLs1bIndex(enPassantAttacks);
								addMove(moveList, encodeMove(sourceSquare, targetEnPassant, piece, 0, 1, 0, 1, 0));
							}
						}
						popBit(bb, sourceSquare);
					}
				}
				if (!captures) {
					if (piece == Piece::K) {
						if (pos.castle & CastlingRights::WK) {
							if (!getBit(Bitboards::occupancies[Colors::both], f1) && !getBit(Bitboards::occupancies[Colors::both], g1)) {
								if (!pos.isSquareAttacked(e1, Colors::black) && !pos.isSquareAttacked(f1, Colors::black)) {
									addMove(moveList, encodeMove(e1, g1, piece, 0, 0, 0, 0, 1));
								}
							}
						}
						if (pos.castle & CastlingRights::WQ) {
							if (!getBit(Bitboards::occupancies[Colors::both], d1) && !getBit(Bitboards::occupancies[Colors::both], c1) && !getBit(Bitboards::occupancies[Colors::both], b1)) {
								if (!pos.isSquareAttacked(e1, Colors::black) && !pos.isSquareAttacked(d1, Colors::black)) {
									addMove(moveList, encodeMove(e1, c1, piece, 0, 0, 0, 0, 1));
								}
							}
						}
					}
				}
			} else {
				if (piece == Piece::p) {
					while (bb) {
						sourceSquare = Bitboards::getLs1bIndex(bb);
						target = sourceSquare + 8;
						if (!(target > h1) && !getBit(Bitboards::occupancies[both], target)) {
							if (captures && sourceSquare >= a2 && sourceSquare <= h2) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::q, 0, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::r, 0, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::b, 0, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::n, 0, 0, 0, 0));
							} else {
								if (sourceSquare >= a2 && sourceSquare <= h2) {
									addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::q, 0, 0, 0, 0));
									addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::r, 0, 0, 0, 0));
									addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::b, 0, 0, 0, 0));
									addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::n, 0, 0, 0, 0));
								} else {
									addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 0, 0, 0, 0));
									if ((sourceSquare >= a7 && sourceSquare <= h7) && !getBit(Bitboards::occupancies[both], (target + 8))) {
										addMove(moveList, encodeMove(sourceSquare, (target + 8), piece, 0, 0, 1, 0, 0));
									}
								}
							}
						}
						U64 pawnLookup = Bitboards::pawnAttacks[pos.sideToMove][sourceSquare];
						attacks = pawnLookup & Bitboards::occupancies[Colors::white];
						while (attacks) {
							target = Bitboards::getLs1bIndex(attacks);
							if (sourceSquare >= a2 && sourceSquare <= h2) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::q, 1, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::r, 1, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::b, 1, 0, 0, 0));
								addMove(moveList, encodeMove(sourceSquare, target, piece, Piece::n, 1, 0, 0, 0));
							} else {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
							popBit(attacks, target);
						}
						if (pos.enPassant != no_sq) {
							U64 enPassantAttacks = Bitboards::pawnAttacks[pos.sideToMove][sourceSquare] & (1ULL << pos.enPassant);
							if (enPassantAttacks) {
								int targetEnPassant = Bitboards::getLs1bIndex(enPassantAttacks);
								addMove(moveList, encodeMove(sourceSquare, targetEnPassant, piece, 0, 1, 0, 1, 0));
							}
						}
						popBit(bb, sourceSquare);
					}
				}
				if (!captures) {
					if (piece == Piece::k) {
						if (pos.castle & CastlingRights::BK) {
							if (!getBit(Bitboards::occupancies[Colors::both], f8) && !getBit(Bitboards::occupancies[Colors::both], g8)) {
								if (!pos.isSquareAttacked(e8, Colors::white) && !pos.isSquareAttacked(f8, Colors::white)) {
									addMove(moveList, encodeMove(e8, g8, piece, 0, 0, 0, 0, 1));
								}
							}
						}
						if (pos.castle & CastlingRights::BQ) {
							if (!getBit(Bitboards::occupancies[Colors::both], d8) && !getBit(Bitboards::occupancies[Colors::both], c8) && !getBit(Bitboards::occupancies[Colors::both], b8)) {
								if (!pos.isSquareAttacked(e8, Colors::white) && !pos.isSquareAttacked(d8, Colors::white)) {
									addMove(moveList, encodeMove(e8, c8, piece, 0, 0, 0, 0, 1));
								}
							}
						}
					}
				}
			}
			if ((pos.sideToMove == Colors::white) ? piece == Piece::N : piece == Piece::n) {
				while (bb) {
					sourceSquare = Bitboards::getLs1bIndex(bb);
					attacks = Bitboards::knightAttacks[sourceSquare] & ((pos.sideToMove == Colors::white) ? ~Bitboards::occupancies[Colors::white] : ~Bitboards::occupancies[Colors::black]);
					while (attacks) {
						target = Bitboards::getLs1bIndex(attacks);
						if (captures) {
							if (getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						} else {
							if (!getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 0, 0, 0, 0));
							} else {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						}
						popBit(attacks, target);
					}
					popBit(bb, sourceSquare);
				}
			}
			if ((pos.sideToMove == Colors::white) ? piece == Piece::B : piece == Piece::b) {
				while (bb) {
					sourceSquare = Bitboards::getLs1bIndex(bb);
					attacks = Magic::getBishopAttacks(sourceSquare, Bitboards::occupancies[Colors::both]) & ((pos.sideToMove == Colors::white) ? ~Bitboards::occupancies[Colors::white] : ~Bitboards::occupancies[Colors::black]);
					while (attacks) {
						target = Bitboards::getLs1bIndex(attacks);
						if (captures) {
							if (getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						} else {
							if (!getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 0, 0, 0, 0));
							} else {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						}
						popBit(attacks, target);
					}
					popBit(bb, sourceSquare);
				}
			}
			if ((pos.sideToMove == Colors::white) ? piece == Piece::R : piece == Piece::r) {
				while (bb) {
					sourceSquare = Bitboards::getLs1bIndex(bb);
					attacks = Magic::getRookAttacks(sourceSquare, Bitboards::occupancies[Colors::both]) & ((pos.sideToMove == Colors::white) ? ~Bitboards::occupancies[Colors::white] : ~Bitboards::occupancies[Colors::black]);
					while (attacks) {
						target = Bitboards::getLs1bIndex(attacks);
						if (captures) {
							if (getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						} else {
							if (!getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 0, 0, 0, 0));
							} else {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						}
						popBit(attacks, target);
					}
					popBit(bb, sourceSquare);
				}
			}
			if ((pos.sideToMove == Colors::white) ? piece == Piece::Q : piece == Piece::q) {
				while (bb) {
					sourceSquare = Bitboards::getLs1bIndex(bb);
					attacks = Magic::getQueenAttacks(sourceSquare, Bitboards::occupancies[Colors::both]) & ((pos.sideToMove == Colors::white) ? ~Bitboards::occupancies[Colors::white] : ~Bitboards::occupancies[Colors::black]);
					while (attacks) {
						target = Bitboards::getLs1bIndex(attacks);
						if (captures) {
							if (getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						} else {
							if (!getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 0, 0, 0, 0));
							} else {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						}
						popBit(attacks, target);
					}
					popBit(bb, sourceSquare);
				}
			}
			if ((pos.sideToMove == Colors::white) ? piece == Piece::K : piece == Piece::k) {
				while (bb) {
					sourceSquare = Bitboards::getLs1bIndex(bb);
					attacks = Bitboards::kingAttacks[sourceSquare] & ((pos.sideToMove == Colors::white) ? ~Bitboards::occupancies[Colors::white] : ~Bitboards::occupancies[Colors::black]);
					while (attacks) {
						target = Bitboards::getLs1bIndex(attacks);
						if (captures) {
							if (getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						} else {
							if (!getBit(((pos.sideToMove == Colors::white) ? Bitboards::occupancies[Colors::black] : Bitboards::occupancies[Colors::white]), target)) {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 0, 0, 0, 0));
							} else {
								addMove(moveList, encodeMove(sourceSquare, target, piece, 0, 1, 0, 0, 0));
							}
						}
						popBit(attacks, target);
					}
					popBit(bb, sourceSquare);
				}
			}
		}
	}
}


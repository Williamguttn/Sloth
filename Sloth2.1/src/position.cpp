#include <iostream>
#include <string>

#include "position.h"
#include "bitboards.h"
#include "magic.h"
#include "piece.h"
#include "movegen.h"
#include "evaluate.h"
#include "search.h"
#include "types.h"

using namespace Sloth::Bitboards;

namespace Sloth {

	U64 Zobrist::pieceKeys[12][64];
	U64 Zobrist::enPassantKeys[64];
	U64 Zobrist::castlingKeys[16];
	U64 Zobrist::sideKey;

	void Zobrist::initRandomKeys() {

		Magic::randomState = 1804289383;

		for (int piece = Piece::P; piece <= Piece::k; piece++) {
			for (int sq = 0; sq < 64; sq++) {
				Zobrist::pieceKeys[piece][sq] = Magic::getRandomU64Num();
			}
		}

		for (int sq = 0; sq < 64; sq++) {
			Zobrist::enPassantKeys[sq] = Magic::getRandomU64Num();
		}

		for (int i = 0; i < 16; i++) {
			Zobrist::castlingKeys[i] = Magic::getRandomU64Num();
		}

		Zobrist::sideKey = Magic::getRandomU64Num();
	}

	U64 Zobrist::generateHashKey(Position& pos) { // generate unique hash key
		U64 finalKey = 0ULL;
		U64 bb;

		for (int piece = Piece::P; piece <= Piece::k; piece++) {
			bb = Bitboards::bitboards[piece];

			while (bb) {
				int sq = Bitboards::getLs1bIndex(bb);

				finalKey ^= pieceKeys[piece][sq];

				popBit(bb, sq);
			}
		}

		if (pos.enPassant != no_sq) {
			finalKey ^= enPassantKeys[pos.enPassant];
		}

		finalKey ^= castlingKeys[pos.castle];

		if (pos.sideToMove == Colors::black) finalKey ^= sideKey;

		return finalKey;
	}

	int Position::makeMove(Position& pos, int move, int moveFlag) {
		// quiet
		if (moveFlag == MoveType::allMoves) {
			copyBoard(pos);

			int sourceSquare = getMoveSource(move);
			int targetSquare = getMoveTarget(move);
			int piece = getMovePiece(move);
			int promotedPiece = getMovePromotion(move);
			int captureFlag = getMoveCapture(move);
			int doubleFlag = getDoublePush(move);
			int enPassantFlag = getMoveEnpassant(move);
			int castlingFlag = getMoveCastling(move);

			popBit(Bitboards::bitboards[piece], sourceSquare); // pop bit from sourcesquare
			setBit(Bitboards::bitboards[piece], targetSquare); // set bit on targetsquare

			// hash the piece (move the piece in hash)
			hashKey ^= Zobrist::pieceKeys[piece][sourceSquare];
			hashKey ^= Zobrist::pieceKeys[piece][targetSquare];

			pos.fifty++;

			if (piece == Piece::P || piece == Piece::p) {
				pos.fifty = 0;
			}

			if (captureFlag) { // if move is capturing something
				int startPiece, endPiece;

				startPiece = (pos.sideToMove == Colors::white) ? Piece::p : Piece::P;
				endPiece = (pos.sideToMove == Colors::white) ? Piece::k : Piece::K;

				pos.fifty = 0;

				// loop over bb opposite to current side to move
				for (int bbPiece = startPiece; bbPiece <= endPiece; bbPiece++) {
					if (getBit(Bitboards::bitboards[bbPiece], targetSquare)) { // if piece on target square, then remove from corresponding bitboard
						popBit(Bitboards::bitboards[bbPiece], targetSquare);

						// remove the piece from hash
						hashKey ^= Zobrist::pieceKeys[bbPiece][targetSquare];

						break;
					}
				}
			}

			// pawn promotions
			if (promotedPiece) {
				popBit(Bitboards::bitboards[(pos.sideToMove == Colors::white) ? Piece::P : Piece::p], targetSquare);// remove pawn from target square

				hashKey ^= Zobrist::pieceKeys[(pos.sideToMove == Colors::white) ? Piece::P : Piece::p][targetSquare]; // remove from hash key

				setBit(Bitboards::bitboards[promotedPiece], targetSquare); // set up the promoted piece

				hashKey ^= Zobrist::pieceKeys[promotedPiece][targetSquare]; // adding promoted to hash key
			}

			if (enPassantFlag) {

				if (pos.sideToMove == Colors::white) {
					popBit(Bitboards::bitboards[Piece::p], targetSquare + 8);

					hashKey ^= Zobrist::pieceKeys[Piece::p][targetSquare + 8]; // remove from hash key
				}
				else {
					popBit(Bitboards::bitboards[Piece::P], targetSquare - 8);

					hashKey ^= Zobrist::pieceKeys[Piece::P][targetSquare - 8];
				}
			}

			// hash enpassant (remove enpassant square from hash key)
			if (pos.enPassant != no_sq) hashKey ^= Zobrist::enPassantKeys[pos.enPassant];

			// reset enpassant square regardless of what is moved
			pos.enPassant = no_sq;

			if (doubleFlag) { // take a look at this is perft test fails

				if (pos.sideToMove == Colors::white) {
					pos.enPassant = targetSquare + 8;

					hashKey ^= Zobrist::enPassantKeys[targetSquare + 8];
				}
				else {
					pos.enPassant = targetSquare - 8;

					hashKey ^= Zobrist::enPassantKeys[targetSquare - 8];
				}
			}

			if (castlingFlag) {
				switch (targetSquare)
				{
				case (g1): // king side
					popBit(Bitboards::bitboards[Piece::R], h1); // remove rook from h1
					setBit(Bitboards::bitboards[Piece::R], f1); // set it to f1

					hashKey ^= Zobrist::pieceKeys[Piece::R][h1]; // hashing the rook
					hashKey ^= Zobrist::pieceKeys[Piece::R][f1];
					break;
				case (c1):
					popBit(Bitboards::bitboards[Piece::R], a1);
					setBit(Bitboards::bitboards[Piece::R], d1);

					hashKey ^= Zobrist::pieceKeys[Piece::R][a1];
					hashKey ^= Zobrist::pieceKeys[Piece::R][d1];
					break;
				case (g8): // black
					popBit(Bitboards::bitboards[Piece::r], h8);
					setBit(Bitboards::bitboards[Piece::r], f8);

					hashKey ^= Zobrist::pieceKeys[Piece::r][h8];
					hashKey ^= Zobrist::pieceKeys[Piece::r][f8];
					break;
				case (c8):
					popBit(Bitboards::bitboards[Piece::r], a8);
					setBit(Bitboards::bitboards[Piece::r], d8);

					hashKey ^= Zobrist::pieceKeys[Piece::r][a8];
					hashKey ^= Zobrist::pieceKeys[Piece::r][d8];
					break;
				default:
					break;
				}
			}

			// hash castling
			hashKey ^= Zobrist::castlingKeys[pos.castle];

			// update castling rights (white a1 rook might be messing it up???)
			pos.castle &= CASTLING_RIGHTS_CONSTANTS[sourceSquare];
			pos.castle &= CASTLING_RIGHTS_CONSTANTS[targetSquare];
			hashKey ^= Zobrist::castlingKeys[pos.castle];

			// update occupancies
			memset(Bitboards::occupancies, 0ULL, 24);

			for (int bPiece = Piece::P; bPiece <= Piece::K; bPiece++) {
				Bitboards::occupancies[Colors::white] |= Bitboards::bitboards[bPiece];
			}

			for (int bPiece = Piece::p; bPiece <= Piece::k; bPiece++) {
				Bitboards::occupancies[Colors::black] |= Bitboards::bitboards[bPiece];
			}

			Bitboards::occupancies[Colors::both] |= Bitboards::occupancies[Colors::white];
			Bitboards::occupancies[Colors::both] |= Bitboards::occupancies[Colors::black];

			pos.sideToMove ^= 1;

			hashKey ^= Zobrist::sideKey;

			// make sure that king hasnt been exposed into a check
			if (isSquareAttacked((pos.sideToMove == Colors::white) ? Bitboards::getLs1bIndex(Bitboards::bitboards[Piece::k]) : Bitboards::getLs1bIndex(Bitboards::bitboards[Piece::K]), pos.sideToMove)) {
				takeBack(pos);

				return 0;
			}
			else
				return 1;
		}
		else {
			// capture
			if (getMoveCapture(move)) {
				return makeMove(pos, move, MoveType::allMoves);   // added return - fixes crash/bad performance in GCC & Clang - JA
				
			}
			else
				return 0; // dont make the move
		}
	}

	Position Position::parseFen(const char* fen) { // Will technically load the position
		memset(Bitboards::bitboards, 0ULL, sizeof(Bitboards::bitboards)); // reset board position and state variables
		memset(Bitboards::occupancies, 0ULL, sizeof(Bitboards::occupancies));

		sideToMove = 0;
		enPassant = no_sq;
		castle = 0;

		hashKey = 0ULL;

		fifty = 0;

		Search::repetitionIndex = 0;
		memset(Search::repetitionTable, 0ULL, sizeof(Search::repetitionTable));

		//printf("\nply: %d\n", Search::ply);

		Search::ply = 0;

		for (int r = 0; r < 8; r++) {
			for (int f = 0; f < 8; f++) {
				int sq = r * 8 + f;

				if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
					int piece = Piece::charToPiece(*fen);

					setBit(Bitboards::bitboards[piece], sq);

					fen++;
				}

				if (*fen >= '0' && *fen <= '9') {
					int offset = *fen - '0';

					int piece = -1;

					for (int bbPiece = Piece::P; bbPiece <= Piece::k; bbPiece++) {
						if (getBit(Bitboards::bitboards[bbPiece], sq)) {
							piece = bbPiece;
						}
					}

					if (piece == -1)
						f--;

					f += offset;

					fen++;
				}

				if (*fen == '/') fen++;
			}
		}

		fen++;

		// Parse side to move
		(*fen == 'w') ? (sideToMove = Colors::white) : (sideToMove = Colors::black);

		// Parse castling rights
		fen += 2;

		while (*fen != ' ') {
			switch (*fen) {
			case 'K': castle |= WK; break;
			case 'Q': castle |= WQ; break;
			case 'k': castle |= BK; break;
			case 'q': castle |= BQ; break;
			case '-': break;
			default: break;
			}

			fen++;
		}

		// Parse en passant square
		fen++;

		if (*fen != '-') { // there's an en passant square
			int file = fen[0] - 'a';
			int rank = 8 - (fen[1] - '0');

			enPassant = rank * 8 + file;
		}
		else
			enPassant = no_sq;

		fen++;

		fifty = atoi(fen);

		// white pieces bitboards
		for (int piece = Piece::P; piece <= Piece::K; piece++) {
			Bitboards::occupancies[white] |= Bitboards::bitboards[piece];
		}

		// black pieces bitboards
		for (int piece = Piece::p; piece <= Piece::k; piece++) {
			Bitboards::occupancies[black] |= Bitboards::bitboards[piece];
		}

		Bitboards::occupancies[both] = (Bitboards::occupancies[white] | Bitboards::occupancies[black]);

		hashKey = Zobrist::generateHashKey(*this);

		return *this;
	}

	void Position::printBoard() {
		std::cout << std::endl;

		for (int r = 0; r < 8; r++) {
			for (int f = 0; f < 8; f++) {
				int sq = r * 8 + f;

				if (!f) {
					printf("  %d ", 8 - r);
				}

				int piece = -1;

				for (int bbPiece = Piece::P; bbPiece <= Piece::k; bbPiece++) {
					if (getBit(Bitboards::bitboards[bbPiece], sq)) { // if piece on current square
						piece = bbPiece;
					}
				}

				printf(" %c", (piece == -1) ? '.' : Piece::asciiPieces[piece]);
			}
			printf("\n");
		}

		printf("\n     a b c d e f g h\n\n");

		printf("     Side:     %s\n", !sideToMove ? "white" : "black");

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

		printf("     Enpassant:   %s\n", (enPassant != no_sq) ? squareToCoordinates[enPassant] : "no");

		printf("     Castling:  %c%c%c%c\n\n", (castle & WK) ? 'K' : '-',
			(castle & WK) ? 'Q' : '-',
			(castle & BK) ? 'k' : '-',
			(castle & BQ) ? 'q' : '-');

		printf("     Hash key: %llx\n", hashKey);
	}

	inline int Position::isSquareAttacked(int square, int side) {
		// attacked by white pawns
		if ((side == Colors::white) && (Bitboards::pawnAttacks[Colors::black][square] & Bitboards::bitboards[Piece::P]))
			return 1;

		if ((side == Colors::black) && (Bitboards::pawnAttacks[Colors::white][square] & Bitboards::bitboards[Piece::p]))
			return 1;

		if (Bitboards::knightAttacks[square] & ((side == Colors::white) ? Bitboards::bitboards[Piece::N] : Bitboards::bitboards[Piece::n]))
			return 1;

		if (Magic::getBishopAttacks(square, Bitboards::occupancies[Colors::both]) & ((side == Colors::white) ? Bitboards::bitboards[Piece::B] : Bitboards::bitboards[Piece::b])) return 1;

		if (Magic::getRookAttacks(square, Bitboards::occupancies[Colors::both]) & ((side == Colors::white) ? Bitboards::bitboards[Piece::R] : Bitboards::bitboards[Piece::r]))
			return 1;

		if (Magic::getQueenAttacks(square, Bitboards::occupancies[Colors::both]) & ((side == Colors::white) ? Bitboards::bitboards[Piece::Q] : Bitboards::bitboards[Piece::q]))
			return 1;

		if (Bitboards::kingAttacks[square] & ((side == Colors::white) ? Bitboards::bitboards[Piece::K] : Bitboards::bitboards[Piece::k]))
			return 1;

		return 0;
	}

	U64 Position::attackersTo(int sq, U64 occ) {
		return (bitboards[Piece::P] & Bitboards::pawnAttacks[black][sq]) |
			(bitboards[Piece::p] & Bitboards::pawnAttacks[white][sq]) |
			(knightAttacks[sq] & (bitboards[Piece::N] | bitboards[Piece::n])) |
			(kingAttacks[sq] & (bitboards[Piece::K] | bitboards[Piece::k])) |
			(Magic::getRookAttacks(sq, occ) & (bitboards[Piece::R] | bitboards[Piece::r] | bitboards[Piece::Q] | bitboards[Piece::q])) |
			(Magic::getBishopAttacks(sq, occ) & (bitboards[Piece::B] | bitboards[Piece::b] | bitboards[Piece::Q] | bitboards[Piece::q]));
	}

	U64 Position::pawnAttacks(int color) {
		U64 result = 0;

		// Calculate pawn attacks
		U64 pawns = bitboards[color == Colors::white ? Piece::P : Piece::p];
		while (pawns) {
			int index = getLs1bIndex(pawns);
			result |= Bitboards::pawnAttacks[color][index];
			pawns &= pawns - 1;
		}

		return result;
	}

	U64 Position::attackedBy(int color) { // squares attacked by pieces of color
		U64 result = 0;
		U64 kingSquare = bitboards[color == Colors::white ? Piece::K : Piece::k];
		U64 occ = occupancies[both] ^ kingSquare;

		result |= kingAttacks[getLs1bIndex(kingSquare)];

		U64 knights = bitboards[color == Colors::white ? Piece::N : Piece::n];
		while (knights) {
			int index = getLs1bIndex(knights);
			result |= knightAttacks[index];
			knights &= knights - 1;
		}

		U64 pawns = bitboards[color == Colors::white ? Piece::P : Piece::p];
		while (pawns) {
			int index = getLs1bIndex(pawns);
			result |= Bitboards::pawnAttacks[color][index];
			pawns &= pawns - 1;
		}

		U64 rooks = bitboards[color == Colors::white ? Piece::R : Piece::r];
		while (rooks) {
			int index = getLs1bIndex(rooks);
			result |= Magic::getRookAttacks(index, occ);
			rooks &= rooks - 1;
		}

		U64 bishops = bitboards[color == Colors::white ? Piece::B : Piece::b];
		while (bishops) {
			int index = getLs1bIndex(bishops);
			result |= Magic::getBishopAttacks(index, occ);
			bishops &= bishops - 1;
		}

		U64 queens = bitboards[color == Colors::white ? Piece::Q : Piece::q];
		while (queens) {
			int index = getLs1bIndex(queens);
			result |= Magic::getQueenAttacks(index, occ);
			queens &= queens - 1;
		}

		return result;
	}

	U64 Position::attackedTwice(int color) {
		U64 attacked = 0;
		U64 attackedTwice = 0;

		auto updateAttacks = [&](U64 newAttacks) {
			attackedTwice |= attacked & newAttacks;
			attacked |= newAttacks;
		};

		U64 kingSquare = bitboards[color == Colors::white ? Piece::K : Piece::k];
		updateAttacks(kingAttacks[getLs1bIndex(kingSquare)]);

		U64 knights = bitboards[color == Colors::white ? Piece::N : Piece::n];
		while (knights) {
			int index = getLs1bIndex(knights);
			updateAttacks(knightAttacks[index]);
			knights &= knights - 1;
		}

		U64 pawns = bitboards[color == Colors::white ? Piece::P : Piece::p];
		while (pawns) {
			int index = getLs1bIndex(pawns);
			updateAttacks(Bitboards::pawnAttacks[color][index]);
			pawns &= pawns - 1;
		}

		U64 occ = occupancies[both] ^ kingSquare;

		U64 rooks = bitboards[color == Colors::white ? Piece::R : Piece::r];
		while (rooks) {
			int index = getLs1bIndex(rooks);
			updateAttacks(Magic::getRookAttacks(index, occ));
			rooks &= rooks - 1;
		}

		U64 bishops = bitboards[color == Colors::white ? Piece::B : Piece::b];
		while (bishops) {
			int index = getLs1bIndex(bishops);
			updateAttacks(Magic::getBishopAttacks(index, occ));
			bishops &= bishops - 1;
		}

		U64 queens = bitboards[color == Colors::white ? Piece::Q : Piece::q];
		while (queens) {
			int index = getLs1bIndex(queens);
			updateAttacks(Magic::getQueenAttacks(index, occ));
			queens &= queens - 1;
		}

		return attackedTwice;
	}

	void Position::printAttackedSquares(int side) {
		std::cout << std::endl;

		for (int r = 0; r < 8; r++) {
			for (int f = 0; f < 8; f++) {
				int sq = r * 8 + f;

				if (!f)
					printf("  %d ", 8 - r);

				printf(" %d", isSquareAttacked(sq, side) ? 1 : 0);
			}
			printf("\n");
		}

		printf("\n     a b c d e f g h\n\n");
	}

	Position game;
}
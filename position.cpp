#include <iostream>
#include <string>

#include "position.h"
#include "bitboards.h"
#include "magic.h"
#include "piece.h"
#include "movegen.h"
#include "types.h"

namespace Sloth {

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

			if (captureFlag) { // if move is capturing something
				int startPiece, endPiece;

				startPiece = (pos.sideToMove == Colors::white) ? Piece::p : Piece::P;
				endPiece = (pos.sideToMove == Colors::white) ? Piece::k : Piece::K;

				// loop over bb opposite to current side to move
				for (int bbPiece = startPiece; bbPiece <= endPiece; bbPiece++) {
					if (getBit(Bitboards::bitboards[bbPiece], targetSquare)) { // if piece on target square, then remove from corresponding bitboard
						popBit(Bitboards::bitboards[bbPiece], targetSquare);

						break;
					}
				}
			}

			// pawn promotions
			if (promotedPiece) {
				popBit(Bitboards::bitboards[(pos.sideToMove == Colors::white) ? Piece::P : Piece::p], targetSquare);// remove pawn from target square

				setBit(Bitboards::bitboards[promotedPiece], targetSquare); // set up the promoted piece
			}

			if (enPassantFlag) {
				(pos.sideToMove == Colors::white) ? popBit(Bitboards::bitboards[Piece::p], targetSquare + 8) : popBit(Bitboards::bitboards[Piece::P], targetSquare - 8);
			}

			// reset enpassant square regardless of what is moved
			pos.enPassant = no_sq;

			if (doubleFlag) { // take a look at this is perft test fails
				(pos.sideToMove == Colors::white) ? (pos.enPassant = targetSquare + 8) : (pos.enPassant = targetSquare - 8);
			}

			if (castlingFlag) {
				switch (targetSquare)
				{
				case (g1): // king side
					popBit(Bitboards::bitboards[Piece::R], h1); // remove rook from h1
					setBit(Bitboards::bitboards[Piece::R], f1); // set it to f1
					break;
				case (c1):
					popBit(Bitboards::bitboards[Piece::R], a1);
					setBit(Bitboards::bitboards[Piece::R], d1);
					break;
				case (g8): // black
					popBit(Bitboards::bitboards[Piece::r], h8);
					setBit(Bitboards::bitboards[Piece::r], f8);
					break;
				case (c8):
					popBit(Bitboards::bitboards[Piece::r], a8);
					setBit(Bitboards::bitboards[Piece::r], d8);
					break;
				default:
					break;
				}
			}

			// update castling rights (white a1 rook might be messing it up???)
			pos.castle &= CASTLING_RIGHTS_CONSTANTS[sourceSquare];
			pos.castle &= CASTLING_RIGHTS_CONSTANTS[targetSquare];

			// update occupancies
			memset(Bitboards::occupancies, 0ULL, 24);
			
			// DOING THIS FOR NOW, MIGHT BE A QUICKER SOLUTION TO SPEED UP THE MAKEMOVE FUNCTION (like only having one loop or whatever)
			for (int bPiece = Piece::P; bPiece <= Piece::K; bPiece++) {
				Bitboards::occupancies[Colors::white] |= Bitboards::bitboards[bPiece];
			}

			for (int bPiece = Piece::p; bPiece <= Piece::k; bPiece++) {
				Bitboards::occupancies[Colors::black] |= Bitboards::bitboards[bPiece];
			}

			Bitboards::occupancies[Colors::both] |= Bitboards::occupancies[Colors::white];
			Bitboards::occupancies[Colors::both] |= Bitboards::occupancies[Colors::black];

			pos.sideToMove ^= 1;

			// make sure that king hasnt been exposed into a check
			// THIS MIGHT TAKE UP MUCH PERFORMANCE
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
				makeMove(pos, move, MoveType::allMoves);
			}
			else
				return 0; // dont make the move
		}
	}

	Position Position::parseFen(const char *fen) { // Will technically load the position
		memset(Bitboards::bitboards, 0ULL, sizeof(Bitboards::bitboards)); // reset board position and state variables
		memset(Bitboards::occupancies, 0ULL, sizeof(Bitboards::occupancies));

		sideToMove = 0;
		enPassant = no_sq;
		castle = 0;

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

		// white pieces bitboards
		for (int piece = Piece::P; piece <= Piece::K; piece++) {
			Bitboards::occupancies[white] |= Bitboards::bitboards[piece];
		}

		// black pieces bitboards
		for (int piece = Piece::p; piece <= Piece::k; piece++) {
			Bitboards::occupancies[black] |= Bitboards::bitboards[piece];
		}

		Bitboards::occupancies[both] = (Bitboards::occupancies[white] | Bitboards::occupancies[black]);

		//printf("Fen: \"%s\"\n", fen);

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

			printf("     Enpassant:   %s\n", (enPassant != SQ_NONE) ? squareToCoordinates[enPassant] : "no");

			printf("     Castling:  %c%c%c%c\n\n", (castle & WK) ? 'K' : '-',
				(castle & WK) ? 'Q' : '-',
				(castle & BK) ? 'k' : '-',
				(castle & BQ) ? 'q' : '-');
	}

	// FOR LATER: this function might be slow. Attempt to make it more efficient with less calculations if possible.
	inline int Position::isSquareAttacked(int square, int side) {
		// attacked by white pawns
		if ((side == Colors::white) && (Bitboards::pawnAttacks[Colors::black][square] & Bitboards::bitboards[Piece::P]))
			return 1;

		if ((side == Colors::black) && (Bitboards::pawnAttacks[Colors::white][square] & Bitboards::bitboards[Piece::p]))
			return 1;

		if (Bitboards::knightAttacks[square] & ((side == Colors::white) ? Bitboards::bitboards[Piece::N] : Bitboards::bitboards[Piece::n]))
			return 1;

		// KEEP IN MIND: THIS ONE WAS OBSERVED TO NOT WORK PROPERLY ON ATTACKS. PRINT ATTACKED SQUARES WITH BISHOP ON BOARD TO TEST
		//if (Magic::getBishopAttacks(square, Bitboards::occupancies[Colors::both]) & ((!side) ? Bitboards::bitboards[Piece::B] : Bitboards::bitboards[Piece::b]))
			//return 1;
		if (Magic::getBishopAttacks(square, Bitboards::occupancies[Colors::both]) & ((side == Colors::white) ? Bitboards::bitboards[Piece::B] : Bitboards::bitboards[Piece::b])) return 1;

		if (Magic::getRookAttacks(square, Bitboards::occupancies[Colors::both]) & ((side == Colors::white) ? Bitboards::bitboards[Piece::R] : Bitboards::bitboards[Piece::r]))
			return 1;

		if (Magic::getQueenAttacks(square, Bitboards::occupancies[Colors::both]) & ((side == Colors::white) ? Bitboards::bitboards[Piece::Q] : Bitboards::bitboards[Piece::q]))
			return 1;

		if (Bitboards::kingAttacks[square] & ((side == Colors::white) ? Bitboards::bitboards[Piece::K] : Bitboards::bitboards[Piece::k]))
			return 1;

		return 0;
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

	Position& Position::LoadPosition(std::string& fen) {
		int rank = 7; // Start with the 8th rank (top rank)
		int file = 0; // Start with the a-file (leftmost file)

		for (char c : fen) {
			if (c == ' ') {
				break; // Stop parsing at the end of the piece placement section
			}
			else if (c == '/') {
				// Move to the next rank and reset the file
				rank--;
				file = 0;
			}
			else if (isdigit(c)) {
				// Skip empty squares by advancing the file
				file += (c - '0');
			}
			else {
				U64* bitboard = nullptr;

				switch (c) {
				case 'P': bitboard = &Bitboards::whitePawns; break;
				case 'N': bitboard = &Bitboards::whiteKnights; break;
				case 'B': bitboard = &Bitboards::whiteBishops; break;
				case 'R': bitboard = &Bitboards::whiteRooks; break;
				case 'Q': bitboard = &Bitboards::whiteQueens; break;
				case 'K': bitboard = &Bitboards::whiteKing; break;

				case 'p': bitboard = &Bitboards::blackPawns; break;
				case 'n': bitboard = &Bitboards::blackKnights; break;
				case 'b': bitboard = &Bitboards::blackBishops; break;
				case 'r': bitboard = &Bitboards::blackRooks; break;
				case 'q': bitboard = &Bitboards::blackQueens; break;
				case 'k': bitboard = &Bitboards::blackKing; break;

				default:
					break;
				}

				if (bitboard) {
					// Calculate the squareIndex based on rank and file
					int squareIndex = rank * 8 + file;
					// Set the bit for the piece at the calculated squareIndex
					setBit(*bitboard, squareIndex);
				}

				file++; // Move to the next file
			}
		}

		return *this;
	}

	Position game;
}
#include <algorithm>

#include "evaluate.h"

#include "bitboards.h"
#include "piece.h"
#include "position.h"
#include "magic.h"
#include "types.h"

#define S(x, y) {x, y}

namespace Sloth {

	U64 Eval::fileMasks[64];
	U64 Eval::rankMasks[64];

	U64 Eval::isolatedMasks[64];
	U64 Eval::wPassedMasks[64];
	U64 Eval::bPassedMasks[64];

	struct {
		int gamePhase = -1;
		int phaseScore;
	} phase;

	struct {
		int score = 0, scoreOpening = 0, scoreEndgame = 0;
	} scores;

	struct PieceScore { // score for each piece
		int scoreOpening = 0;
		int scoreEndgame = 0;
	};

	const int doublePawnPenaltyOpening = -5;
	const int doublePawnPenaltyEndgame = -10;

	const int isolatedPawnPenaltyOpening = -5;
	const int isolatedPawnPenaltyEndgame = -10;

	const int semiFile = 10; // score for semi open file
	const int openFile = 15; // score for open file

	const PieceScore RookFile[2] = { S(semiFile, semiFile), S(34, 8) };

	// Scores for passed pawns
	const PieceScore passedPawn[2][2][8] = {
		{{S(0,   0), S(-10,  -1), S(-11,   6), S(-16,   7),
		  S(2,   5), S(24,  -1), S(41,  12), S(0,   0)},
		 {S(0,   0), S(-7,    3), S(-10,  11), S(-14,  11),
		  S(-1,  14), S(29,  14), S(48,  24), S(0,   0)}},
		{{S(0,   0), S(-7,    7), S(-12,   9), S(-15,  14),
		  S(2,  16), S(27,  19), S(65,  31), S(0,   0)},
		 {S(0,   0), S(-7,    6), S(-10,   9), S(-14,  15),
		  S(2,  22), S(24,  42), S(31,  73), S(0,   0)}},
	};

	const PieceScore passedFriendlyDistance[8] = {
		S(0,   0), S(-2,   0), S(0,  -2), S(2,  -6),
		S(3, -10), S(-4, -10), S(-4,  -4), S(0,   0),
	};

	const PieceScore passedEnemyDistance[8] = {
		 S(0,   0), S(2,   0), S(4,   0), S(4,   6),
		S(0,  12), S(0,  18), S(8,  18), S(0,   0),
	};

	const PieceScore knightKingDistScore[4] = {
		S(-9,  -6), S(-12, -20), S(-27, -20), S(-47, -19),
	};

	U64 forwardRanksMasks[2][8];

	static const PieceScore pawnShield[] = {
		{-19, -17},
		{0, -12},
		{15, -18},
		{23, -26}
	};

	static const int bishopUnit = 4;
	static const int queenUnit = 9;

	static const int bishopMobilityOpening = 5;
	static const int bishopMobilityEnd = 5;

	static const int queenMobilityOpening = 1;
	static const int queenMobilityEnd = 2;

	static const int doubledRooks = 5;
	static const int doubledRooksEndgame = 10;

	const int kingShieldBonus = 5;

	enum { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

	const int materialScore[2][12] = { // game phase, piece
		82, 337, 365, 477, 1025, 12000, -82, -337, -365, -477, -1025, -12000,

		94, 281, 297, 512,  936, 12000, -94, -281, -297, -512,  -936, -12000
	};

	const int openingScore = 6192;
	const int endgameScore = 518;

	U64 Eval::setFileRankMask(int fileNum, int rankNum) {
		U64 mask = 0ULL;

		for (int i = 0; i < 8; i++) {
			if (fileNum != -1) {
				mask |= setBit(mask, i * 8 + fileNum);
			}
			else if (rankNum != -1) {
				mask |= setBit(mask, rankNum * 8 + i);
			}
		}

		return mask;
	}

	inline int rankOf(int sq) {
		return sq / 8;
	}

	inline int fileOf(int sq) {
		return sq % 8;
	}

	U64 backwardMasks[64];
	U64 connectedMasks[64];

	int distanceBetween[64][64];

	void Eval::initEvalMasks() {

		for (int sq1 = 0; sq1 < 64; sq1++)
			for (int sq2 = 0; sq2 < 64; sq2++)
				distanceBetween[sq1][sq2] = MAX(std::abs(fileOf(sq1) - fileOf(sq2)), std::abs(rankOf(sq1) - rankOf(sq2)));

		for (int rank = 0; rank < 8; rank++) {
			for (int file = 0; file < 8; file++) {
				int sq = rank * 8 + file;

				fileMasks[sq] |= setFileRankMask(file, -1);
				rankMasks[sq] |= setFileRankMask(-1, rank);

				isolatedMasks[sq] |= setFileRankMask(file - 1, -1);
				isolatedMasks[sq] |= setFileRankMask(file + 1, -1);

				for (int r = rank - 1; r >= 0; r--) {
					if (file > 0) backwardMasks[sq] |= (1ULL << (r * 8 + (file - 1)));
					if (file < 7) backwardMasks[sq] |= (1ULL << (r * 8 + (file + 1)));
				}

				// Initialize connected masks
				if (file > 0) {
					connectedMasks[sq] |= (1ULL << (rank * 8 + (file - 1))); // same rank
					if (rank > 0) connectedMasks[sq] |= (1ULL << ((rank - 1) * 8 + (file - 1))); // rank-1
					if (rank < 7) connectedMasks[sq] |= (1ULL << ((rank + 1) * 8 + (file - 1))); // rank+1
				}
				if (file < 7) {
					connectedMasks[sq] |= (1ULL << (rank * 8 + (file + 1))); // same rank
					if (rank > 0) connectedMasks[sq] |= (1ULL << ((rank - 1) * 8 + (file + 1))); // rank-1
					if (rank < 7) connectedMasks[sq] |= (1ULL << ((rank + 1) * 8 + (file + 1))); // rank+1
				}
			}
		}

		for (int rank = 0; rank < 8; rank++) {
			for (int i = rank; i < 8; i++)
				forwardRanksMasks[Colors::white][rank] |= rankMasks[i];
			forwardRanksMasks[Colors::black][rank] = ~forwardRanksMasks[Colors::white][rank] | rankMasks[rank];
		}

		for (int rank = 0; rank < 8; rank++) {
			for (int file = 0; file < 8; file++) {
				int sq = rank * 8 + file;

				wPassedMasks[sq] |= setFileRankMask(file - 1, -1);
				wPassedMasks[sq] |= setFileRankMask(file, -1);
				wPassedMasks[sq] |= setFileRankMask(file + 1, -1);

				bPassedMasks[sq] = wPassedMasks[sq];

				for (int i = 0; i < (8 - rank); i++) {
					// reset the redundant bits
					wPassedMasks[sq] &= ~rankMasks[(7 - i) * 8 + file];
				}

				for (int i = 0; i < rank + 1; i++) {
					bPassedMasks[sq] &= ~rankMasks[i * 8 + file];
				}
			}
		}
	}

	const int GET_RANK[64] =
	{
		7, 7, 7, 7, 7, 7, 7, 7,
		6, 6, 6, 6, 6, 6, 6, 6,
		5, 5, 5, 5, 5, 5, 5, 5,
		4, 4, 4, 4, 4, 4, 4, 4,
		3, 3, 3, 3, 3, 3, 3, 3,
		2, 2, 2, 2, 2, 2, 2, 2,
		1, 1, 1, 1, 1, 1, 1, 1,
		0, 0, 0, 0, 0, 0, 0, 0
	};

	inline void scorePiece(PieceScore* score, int scoreOpening, int scoreEndgame) {
		score->scoreOpening += scoreOpening;
		score->scoreEndgame += scoreEndgame;
	}

	static inline U64 occupiedOnFile(int square) { // get all pieces on file
		return Bitboards::occupancies[both] & Eval::fileMasks[square];
	}

	static inline U64 occupiedOnRank(int square) {
		return Bitboards::occupancies[both] & Eval::rankMasks[square];
	}

	static inline int squareDistance(int sq1, int sq2) {
		int rankDiff = abs((sq1 / 8) - (sq2 / 8));
		int fileDiff = abs((sq2 % 8) - (sq2 % 8));

		return std::max(rankDiff, fileDiff);
	}

	static inline int getRank(int square) {
		return square / 8;
	}

	static inline int getFile(int square) {
		return square % 8;
	}

	bool testBit(U64 bb, int i) {
		return bb & (1ULL << i);
	}

	U64 pawnAdvance(U64 pawns, U64 occ, int color) {
		return ~occ & (color == Colors::white ? (pawns << 8) : (pawns >> 8));
	}

	inline PieceScore evaluatePawns(int piece, int square, int sideToMove, Position& pos) { // piece variable will switch between black and white pawns
		int doubled = Bitboards::countBits(Bitboards::bitboards[piece] & Eval::fileMasks[square]); // returns the amount of doubled pawns on the board for said piece side
		PieceScore score = { 0 };

		bool white = (piece == Piece::P);

		U64* passedMask = white ? Eval::wPassedMasks : Eval::bPassedMasks;

		score.scoreOpening += POSITIONAL_SCORE[opening][PAWN][white ? square : MIRROR_SCORE[square]];
		score.scoreEndgame += POSITIONAL_SCORE[endgame][PAWN][white ? square : MIRROR_SCORE[square]];

		U64 myPawns = Bitboards::bitboards[white ? Piece::P : Piece::p];
		int ourColor = white ? Colors::white : Colors::black;

		if (doubled > 1) {
			scorePiece(&score, (doubled - 1) * doublePawnPenaltyOpening, (doubled - 1) * doublePawnPenaltyEndgame); // this line adds penalty to the current piece, one for opening, and one for endgame
		}

		if ((Bitboards::bitboards[piece] & Eval::isolatedMasks[square]) == 0) {
			scorePiece(&score, isolatedPawnPenaltyOpening, isolatedPawnPenaltyEndgame);
		}

		// Passed pawn (~26 elo)

		/*
		Techniques borrowed from Ethereal (https://github.com/AndyGrant/Ethereal/)
		*/
		if ((passedMask[square] & Bitboards::bitboards[white ? Piece::p : Piece::P]) == 0) {
			PieceScore eval = { 0 };

			U64 bitboard = pawnAdvance(1ULL << square, 0ULL, ourColor);
			U64 attackedByEnemy = pos.attackedBy(white ? Colors::black : Colors::white);
			int rank = rankOf(square);

			int dist, flag = 0;

			bool canAdvance = !(bitboard & Bitboards::occupancies[Colors::both]);
			bool safeAdvance = !(bitboard & attackedByEnemy);

			eval.scoreOpening += passedPawn[canAdvance][safeAdvance][rank].scoreOpening;
			eval.scoreEndgame += passedPawn[canAdvance][safeAdvance][rank].scoreEndgame;

			// Evaluate dist from our king
			dist = distanceBetween[square][Bitboards::getLs1bIndex(Bitboards::bitboards[white ? Piece::K : Piece::k])];
			eval.scoreOpening += dist * passedFriendlyDistance[rank].scoreOpening;
			eval.scoreEndgame += dist * passedFriendlyDistance[rank].scoreEndgame;

			// Evaluate dist from their king
			dist = distanceBetween[square][Bitboards::getLs1bIndex(Bitboards::bitboards[white ? Piece::k : Piece::K])];
			eval.scoreOpening += dist * passedEnemyDistance[rank].scoreOpening;
			eval.scoreEndgame += dist * passedEnemyDistance[rank].scoreEndgame;

			// Apply bonus when path to promotion is clear
			bitboard = forwardRanksMasks[ourColor][rankOf(square)] & Eval::fileMasks[fileOf(square)];
			flag = !(bitboard & (Bitboards::occupancies[white ? Colors::black : Colors::white] | attackedByEnemy));

			eval.scoreOpening += flag * -47;
			eval.scoreEndgame += flag * 57;

			scorePiece(&score, eval.scoreOpening, eval.scoreEndgame);
		}

		int mobility = Bitboards::countBits(~Bitboards::occupancies[Colors::both] & Bitboards::pawnAdvance(1ULL << square, 0ULL, white ? Colors::white : Colors::black));

		scorePiece(&score, mobility, mobility * 2);

		if ((Bitboards::bitboards[piece] & backwardMasks[square]) == 0) {
			scorePiece(&score, -4, -7);
		}

		if ((Bitboards::bitboards[piece] & connectedMasks[square]) != 0) {
			scorePiece(&score, 4, 10);
		}

		return score;
	}

	inline PieceScore evaluateKnights(int piece, int square) {
		PieceScore score = { 0 };
		bool white = (piece == Piece::N);

		score.scoreOpening += POSITIONAL_SCORE[opening][KNIGHT][white ? square : MIRROR_SCORE[square]];
		score.scoreEndgame += POSITIONAL_SCORE[endgame][KNIGHT][white ? square : MIRROR_SCORE[square]];

		// ~40 elo including this but for bishops
		if (getRank(square) == (white ? 7 : 0)) {
			scorePiece(&score, -5, -5);
		}

		return score;
	}

	inline PieceScore evaluateRooks(int piece, int square) {
		PieceScore score = { 0 };
		bool white = (piece == Piece::R);

		int ourColor = white ? Colors::white : Colors::black;

		score.scoreOpening += POSITIONAL_SCORE[opening][ROOK][white ? square : MIRROR_SCORE[square]];
		score.scoreEndgame += POSITIONAL_SCORE[endgame][ROOK][white ? square : MIRROR_SCORE[square]];

		U64 myPawns = Bitboards::bitboards[white ? Piece::P : Piece::p];
		U64 enemyPawns = Bitboards::bitboards[white ? Piece::p : Piece::P];

		U64 enemyKing = Bitboards::bitboards[white ? Piece::k : Piece::K];


		// open/semi open file evaluation
		// +17 elo compared to the previous -40 ish

		/*
		Techniques borrowed from Ethereal (https://github.com/AndyGrant/Ethereal/)
		*/
		if (!(myPawns & Eval::fileMasks[square])) {
			bool open = !(enemyPawns & Eval::fileMasks[square]);

			scorePiece(&score, RookFile[open].scoreOpening, RookFile[open].scoreEndgame);
		}

		// bonus if friendly rooks are doubled
		U64 rooksOnFile = Bitboards::bitboards[piece] & Eval::fileMasks[square];
		U64 rooksOnRank = Bitboards::bitboards[piece] & Eval::rankMasks[square];

		// rooks are doubled on the file, with no pieces in between
		if (Bitboards::countBits(rooksOnFile) >= 2) {
			if ((occupiedOnFile(square) & ~rooksOnFile) == 0) {
				scorePiece(&score, doubledRooks * Bitboards::countBits(rooksOnFile), doubledRooksEndgame * Bitboards::countBits(rooksOnFile));
			}
		}

		if (Bitboards::countBits(rooksOnRank) >= 2) {
			if ((occupiedOnRank(square) & ~rooksOnRank) == 0) {
				scorePiece(&score, doubledRooks * Bitboards::countBits(rooksOnRank), doubledRooksEndgame * Bitboards::countBits(rooksOnRank));
			}
		}

		return score;
	}

	inline PieceScore getPieceMobility(bool bishop, int square) {
		PieceScore score = { 0 };

		if (bishop) {
			score.scoreOpening += (Bitboards::countBits(Magic::getBishopAttacks(square, Bitboards::occupancies[both])) - bishopUnit) * bishopMobilityOpening;
			score.scoreEndgame += (Bitboards::countBits(Magic::getBishopAttacks(square, Bitboards::occupancies[both])) - bishopUnit) * bishopMobilityEnd;
		}
		else {
			score.scoreOpening += (Bitboards::countBits(Magic::getQueenAttacks(square, Bitboards::occupancies[both])) - queenUnit) * queenMobilityOpening;
			score.scoreEndgame += (Bitboards::countBits(Magic::getQueenAttacks(square, Bitboards::occupancies[both])) - queenUnit) * queenMobilityEnd;
		}

		return score;
	}

	inline PieceScore evaluateBishops(int piece, int square) {
		PieceScore score = { 0 };
		PieceScore mobility = getPieceMobility(true, square);
		bool white = (piece == Piece::B);

		score.scoreOpening += POSITIONAL_SCORE[opening][BISHOP][white ? square : MIRROR_SCORE[square]];
		score.scoreEndgame += POSITIONAL_SCORE[endgame][BISHOP][white ? square : MIRROR_SCORE[square]];

		scorePiece(&score, mobility.scoreOpening, mobility.scoreEndgame);

		if (testBit(pawnAdvance(Bitboards::bitboards[Piece::P] | Bitboards::bitboards[Piece::p], 0ULL, white ? Colors::black : Colors::white), square)) {
			scorePiece(&score, 4, 24);
		}

		// ~40 elo including this but for knights
		if (getRank(square) == (white ? 7 : 0)) {
			scorePiece(&score, -5, -5);
		}

		return score;
	}

	inline PieceScore evaluateQueens(int piece, int square) {
		PieceScore score = { 0 };
		PieceScore mobility = getPieceMobility(false, square);
		bool white = (piece == Piece::Q);

		score.scoreOpening += POSITIONAL_SCORE[opening][QUEEN][white ? square : MIRROR_SCORE[square]];
		score.scoreEndgame += POSITIONAL_SCORE[endgame][QUEEN][white ? square : MIRROR_SCORE[square]];

		scorePiece(&score, mobility.scoreOpening, mobility.scoreEndgame);

		return score;
	}

	inline PieceScore evaluateKings(int piece, int square) {
		PieceScore score = { 0 };
		bool white = (piece == Piece::K);
		int kingRank = white ? getRank(square) : GET_RANK[MIRROR_SCORE[square]];

		score.scoreOpening += POSITIONAL_SCORE[opening][KING][white ? square : MIRROR_SCORE[square]];
		score.scoreEndgame += POSITIONAL_SCORE[endgame][KING][white ? square : MIRROR_SCORE[square]];

		int myKingSq = Bitboards::getLs1bIndex(Bitboards::bitboards[white ? Piece::K : Piece::k]);
		int theirKingSq = Bitboards::getLs1bIndex(Bitboards::bitboards[white ? Piece::k : Piece::K]);

		int distance = squareDistance(myKingSq, theirKingSq);

		int distScore = 10 * (7 - distance);

		if (Bitboards::kingAttacks[square] & Bitboards::bitboards[white ? Piece::p : Piece::P]) {
            scorePiece(&score, -16, 30);
        }

		if (phase.gamePhase != endgame) { // prolly dont need this, however, intention is to make the king be more careful during the active phases of the game
			if ((Bitboards::bitboards[white ? Piece::P : Piece::p] & Eval::fileMasks[square]) == 0) { // king on semi open file
				scorePiece(&score, -semiFile, -semiFile); // -semifile will turn it into a penalty
			}

			if (((Bitboards::bitboards[Piece::P] | Bitboards::bitboards[Piece::p]) & Eval::fileMasks[square]) == 0) {
				scorePiece(&score, -openFile, -openFile);
			}

			if (kingRank == 0) {
				/*
				New king protection
				~21 elo
				*/

				U64 pawnSquares = white ? (square % 8 < 3 ? 0x007000000000000ULL : 0x000E0000000000000ULL) : (square % 8 < 3 ? 0x700 : 0xE000);
				U64 pawns = Bitboards::bitboards[white ? Piece::P : Piece::p] & pawnSquares;
				PieceScore shieldScores = pawnShield[std::min(Bitboards::countBits(pawns), 3)];

				scorePiece(&score, shieldScores.scoreOpening, shieldScores.scoreEndgame);
			}
		}
		else {
			distScore *= 2; // in case of a king queen vs king situation, we encourage the friendly king to move closer so that we can mate

			// force enemy king to the corner for mating
			int enemyFile = getFile(theirKingSq);
			int enemyRank = getRank(theirKingSq);

			int centerDistFile = std::max(3 - enemyFile, enemyFile - 4);
			int centerDistRank = std::max(3 - enemyRank, enemyRank - 4);

			int cornerScore = (centerDistFile + centerDistRank) * 4;

			scorePiece(&score, cornerScore, cornerScore);
		}

		scorePiece(&score, distScore, distScore);

		return score;
	}

	static inline int getGamePhaseScore() {
		// piece scores
		int w = 0;
		int b = 0;

		for (int p = Piece::N; p <= Piece::Q; p++)
			w += Bitboards::countBits(Bitboards::bitboards[p]) * materialScore[opening][p];

		for (int p = Piece::n; p <= Piece::q; p++)
			b += Bitboards::countBits(Bitboards::bitboards[p]) * -materialScore[opening][p];

		return w + b;
	}

	inline bool isDraw(Position& pos) {

		if (Bitboards::countBits(Bitboards::occupancies[Colors::both]) < 5) {
			// king vs king is a draw
			if ((Bitboards::occupancies[Colors::both] & ~(Bitboards::bitboards[Piece::K] | Bitboards::bitboards[Piece::k])) == 0) {
				return true;
			}
		}

		int pawns = Bitboards::countBits(Bitboards::bitboards[Piece::P] | Bitboards::bitboards[Piece::p]);
		int wKnights = Bitboards::countBits(Bitboards::bitboards[Piece::N]);
		int bKnights = Bitboards::countBits(Bitboards::bitboards[Piece::n]);
		int wBishops = Bitboards::countBits(Bitboards::bitboards[Piece::B]);
		int bBishops = Bitboards::countBits(Bitboards::bitboards[Piece::b]);
		int rooks = Bitboards::countBits(Bitboards::bitboards[Piece::R] | Bitboards::bitboards[Piece::r]);
		int queens = Bitboards::countBits(Bitboards::bitboards[Piece::Q] | Bitboards::bitboards[Piece::q]);

		int minors = wKnights + bKnights + wBishops + bBishops;

		if (rooks + queens + pawns == 0) {
			if (minors == 1) return true;

			if (minors == 2) {
				if (wKnights == 1 && bKnights == 1) return true;

				if (wBishops == 1 && bBishops == 1) return true;
			}
		}

		// try rook vs rook or queen vs queen draw
		if (minors + pawns == 0
			&& ((queens == 0 && Bitboards::countBits(Bitboards::bitboards[Piece::R]) + Bitboards::countBits(Bitboards::bitboards[Piece::r]) == 2)
			|| (rooks == 0 && Bitboards::countBits(Bitboards::bitboards[Piece::Q]) + Bitboards::countBits(Bitboards::bitboards[Piece::q]) == 2))) {
			return true;
		}

		return false;
	}

	inline bool Eval::isEndgame() {
		int phaseScore = getGamePhaseScore();

		return phaseScore < endgameScore;
	}

	inline int Eval::evaluate(Position& pos) {

		scores.score = 0;
		scores.scoreEndgame = 0;
		scores.scoreOpening = 0;

		phase.phaseScore = getGamePhaseScore();

		if (phase.phaseScore > openingScore)
			phase.gamePhase = opening;
		else if (phase.phaseScore < endgameScore)
			phase.gamePhase = endgame;
		else
			phase.gamePhase = middlegame;

		U64 bb;

		int piece, square;

		PieceScore P, N, B, R, Q, K, p, n, b, r, q, k;

		if (phase.gamePhase == endgame) {
			if (isDraw(pos)) return 0;
		}

		for (int bbPiece = Piece::P; bbPiece <= Piece::k; bbPiece++) {
			bb = Bitboards::bitboards[bbPiece];

			while (bb) { // loop over pieces in current bitboard
				piece = bbPiece;

				square = Bitboards::getLs1bIndex(bb);

				scores.scoreOpening += materialScore[opening][piece];
				scores.scoreEndgame += materialScore[endgame][piece];

				switch (piece)
				{
				case Piece::P:
					P = evaluatePawns(Piece::P, square, pos.sideToMove, pos);
					scores.scoreOpening += P.scoreOpening;
					scores.scoreEndgame += P.scoreEndgame;

					break;
				case Piece::N:
					N = evaluateKnights(Piece::N, square);
					scores.scoreOpening += N.scoreOpening;
					scores.scoreEndgame += N.scoreEndgame;

					break;
				case Piece::B:
					B = evaluateBishops(Piece::B, square);
					scores.scoreOpening += B.scoreOpening;
					scores.scoreEndgame += B.scoreEndgame;

					break;
				case Piece::R:
					R = evaluateRooks(Piece::R, square);
					scores.scoreOpening += R.scoreOpening;
					scores.scoreEndgame += R.scoreEndgame;

					break;
				case Piece::Q:
					Q = evaluateQueens(Piece::Q, square);
					scores.scoreOpening += Q.scoreOpening;
					scores.scoreEndgame += Q.scoreEndgame;

					break;
				case Piece::K:
					K = evaluateKings(Piece::K, square);
					scores.scoreOpening += K.scoreOpening;
					scores.scoreEndgame += K.scoreEndgame;

					break;

				case Piece::p:
					p = evaluatePawns(Piece::p, square, pos.sideToMove, pos);
					scores.scoreOpening -= p.scoreOpening;
					scores.scoreEndgame -= p.scoreEndgame;

					break;
				case Piece::n:
					n = evaluateKnights(Piece::n, square);
					scores.scoreOpening -= n.scoreOpening;
					scores.scoreEndgame -= n.scoreEndgame;

					break;
				case Piece::b:
					b = evaluateBishops(Piece::b, square);
					scores.scoreOpening -= b.scoreOpening;
					scores.scoreEndgame -= b.scoreEndgame;

					break;
				case Piece::r:
					r = evaluateRooks(Piece::r, square);
					scores.scoreOpening -= r.scoreOpening;
					scores.scoreEndgame -= r.scoreEndgame;

					break;
				case Piece::q:
					q = evaluateQueens(Piece::q, square);
					scores.scoreOpening -= q.scoreOpening;
					scores.scoreEndgame -= q.scoreEndgame;

					break;
				case Piece::k:
					k = evaluateKings(Piece::k, square);
					scores.scoreOpening -= k.scoreOpening;
					scores.scoreEndgame -= k.scoreEndgame;

					break;
				}

				popBit(bb, square);
			}
		}

		if (phase.gamePhase == middlegame) { // interpolating scores in the middlegame
			/*
				Formula used for calculating interpolated score for a given game phase:
				(openingScore * phaseScore + endgameScore * (openingPhaseScore - gamePhaseScore)) / openingPhaseScore

				EXAMPLE: score for pawn on d4 at phase 5000 would be:
				interpolatedScore = (12 * 5000 + (-7) * (6192 - 5000)) / 6192 = 8.34237726098
			*/
			scores.score = (scores.scoreOpening * phase.phaseScore + scores.scoreEndgame * (openingScore - phase.phaseScore)) / openingScore;
		}
		else if (phase.gamePhase == opening) {
			scores.score = scores.scoreOpening; // pure opening score in opening
		}
		else {
			scores.score = scores.scoreEndgame;
		}

		return (pos.sideToMove == Colors::white) ? scores.score : -scores.score;
	}
}
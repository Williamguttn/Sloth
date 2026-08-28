#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdlib>

#include "search.h"
#include "evaluate.h"
#include "movegen.h"
#include "magic.h"
#include "uci.h"
#include "tune.h"

#undef clamp

#define clamp(x, lower, upper) ((x) < (lower) ? (lower) : ((x) > (upper) ? (upper) : (x)))

namespace Sloth {

	int scoreMove(int move, Position& pos, Threads::ThreadData *threadData);
	static int continuationHistoryScore(int piece, int target, Threads::ThreadData* td);

	int Search::hashEntries = 0;
	HashEntry* Search::hashTable = NULL;

	int Search::contempt = 0;
	
	int totalEntries = 0;
	int usedEntries = 0;
	int lastCurrmoveOutput = 0;
	bool reportedCurrMove = false;
	const int CURRMOVE_INITIAL_DELAY = 2500;
	const int CURRMOVE_INTERVAL = 0;
	const int pieceValues[13] = { 100, 300, 300, 500, 900, VALUE_INFINITE, 100, 300, 300, 500, 900, VALUE_INFINITE, 0 };

	void Search::clearHashTable() {
		//if (game.time.ponder) return;

		HashEntry* hashEntry;
		totalEntries = hashEntries;
		usedEntries = 0;

		for (hashEntry = hashTable; hashEntry < hashTable + hashEntries; hashEntry++) {
			hashEntry->keyXorData.store(0, std::memory_order_relaxed);
			hashEntry->data.store(0, std::memory_order_relaxed);
		}
	}

	// Packs bestMove[0..23] | depth[24..30] | flag[31..32] | score[33..63] into HASHE::data
	static inline uint64_t packHashData(int bestMove, int depth, int flag, int score) {
		// ProbCut can pass a negative depth, and the depth field is unsigned
		if (depth < 0) depth = 0;
		uint64_t data = 0;
		data |= (uint64_t)((uint32_t)bestMove & 0xFFFFFFu);
		data |= (uint64_t)((uint32_t)depth & 0x7Fu) << 24;
		data |= (uint64_t)((uint32_t)flag & 0x3u) << 31;
		data |= (uint64_t)((uint32_t)score & 0x7FFFFFFFu) << 33;
		return data;
	}

	static inline void unpackHashData(uint64_t data, int* bestMove, int* depth, int* flag, int* score) {
		*bestMove = (int)(data & 0xFFFFFFu);
		*depth = (int)((data >> 24) & 0x7Fu);
		*flag = (int)((data >> 31) & 0x3u);
		uint32_t rawScore = (uint32_t)((data >> 33) & 0x7FFFFFFFu);
		if (rawScore & 0x40000000u) rawScore |= 0x80000000u; // sign-extend 31-bit field
		*score = (int)rawScore;
	}

	void Search::initHashTable(int mb) {
		int hashSize = 0x100000 * mb;
		hashEntries = hashSize / sizeof(HashEntry);

		if (hashTable != NULL) {
			printf("info string Clearing hash memory\n");
			free(hashTable);
		}

		hashTable = (HashEntry*)malloc(hashEntries * sizeof(HashEntry));

		totalEntries = mb * 1024 * 1024 / sizeof(HashEntry);
		usedEntries = 0;

		if (hashTable == NULL) {
			printf("info string Couldnt allocate memory for hash table, trying %dMB\n", mb / 2);
			initHashTable(mb / 2);
		} else {
			clearHashTable();
			printf("info string Hash table is initialized with %d entries\n", hashEntries);
		}
	}

	static HASHE* readHashEntry(int alpha, int beta, int* bestMove, int* ttEval, int* ttFlag, int* ttDepth, int depth, Position& pos, bool* hit, Threads::ThreadData* threadData) {

		HASHE* hashEntry = &Search::hashTable[pos.hashKey % Search::hashEntries];
		*hit = false;

		// Lockless hashing, torn combination of old/new fails reconstruction
		uint64_t data = hashEntry->data.load(std::memory_order_relaxed);
		uint64_t storedKeyXor = hashEntry->keyXorData.load(std::memory_order_relaxed);

		if ((storedKeyXor ^ data) != pos.hashKey) return nullptr;

		int storedBestMove, storedDepth, storedFlag, storedScore;
		unpackHashData(data, &storedBestMove, &storedDepth, &storedFlag, &storedScore);

		if (storedDepth >= depth) {
			*bestMove = storedBestMove;

			*ttEval = storedScore;
			*ttFlag = storedFlag;
			*ttDepth = storedDepth;

			int score = storedScore;
			if (score < -MATE_SCORE) score += threadData->ply;
			if (score > MATE_SCORE) score -= threadData->ply;

			if ((storedFlag == hashfEXACT) ||
				(storedFlag == hashfALPHA && score <= alpha) ||
				(storedFlag == hashfBETA && score >= beta)) {
				*hit = true;
				return hashEntry;
			}
		} else {
			*bestMove = storedBestMove;
		}
		return nullptr;
	}

	static void writeHashEntry(int score, int bestMove, int depth, int hashFlag, Position& pos, Threads::ThreadData* threadData) {
		HASHE* hashEntry = &Search::hashTable[pos.hashKey % Search::hashEntries];

		int adjustedScore = score;
		if (adjustedScore < -MATE_SCORE) adjustedScore -= threadData->ply;
		if (adjustedScore > MATE_SCORE) adjustedScore += threadData->ply;

		uint64_t oldData = hashEntry->data.load(std::memory_order_relaxed);
		uint64_t oldKeyXor = hashEntry->keyXorData.load(std::memory_order_relaxed);
		bool sameKey = (oldKeyXor ^ oldData) == pos.hashKey;
		int oldDepth = 0;
		if (sameKey) {
			int unusedMove, unusedFlag, unusedScore;
			unpackHashData(oldData, &unusedMove, &oldDepth, &unusedFlag, &unusedScore);
		}

		if (!sameKey || depth >= oldDepth) {
			uint64_t newData = packHashData(bestMove, depth, hashFlag, adjustedScore);
			hashEntry->data.store(newData, std::memory_order_relaxed);
			hashEntry->keyXorData.store(pos.hashKey ^ newData, std::memory_order_relaxed);
		}
	}

	double hashFull() {
		// Sample a portion of the hash table to estimate usage
		int sampleSize = std::min(1000, Search::hashEntries);
		int used = 0;
		
		for (int i = 0; i < sampleSize; i++) {
			if (Search::hashTable[i].data.load(std::memory_order_relaxed) != 0 ||
				Search::hashTable[i].keyXorData.load(std::memory_order_relaxed) != 0) {
				used++;
			}
		}
		
		return 1000.0 * used / sampleSize;
	}

	static void enablePVScoring(Movegen::MoveList* movelist, Threads::ThreadData* threadData) {
		threadData->followPV = false;

		for (int i = 0; i < movelist->count; i++) {
			if (threadData->pvTable[0][threadData->ply] == movelist->moves[i]) {
				threadData->scorePV = true;
				threadData->followPV = true;
			}
		}
	}

	void Search::printMoveScores(Movegen::MoveList* moveList, Position& pos, Threads::ThreadData* threadData) {
		for (int i = 0; i < moveList->count; i++) {
			int move = moveList->moves[i];
			Movegen::printMove(move);
			printf("score: %d \n", scoreMove(move, pos, threadData));
		}
	}

	static int isRepetition(Position& pos, Threads::ThreadData* threadData) {
		for (int i = 0; i < pos.repetitionIndex; i++) {
			if (pos.repetitionTable[i] == pos.hashKey) {
				return 1;
			}
		}

		return 0;
	}

	static bool isEndgame(Position& pos) {
		int pawnMaterial = Bitboards::countBits(pos.bitboards[Piece::P] | pos.bitboards[Piece::p]) * 100;
		int knightMaterial = Bitboards::countBits(pos.bitboards[Piece::N] | pos.bitboards[Piece::n]) * 320;
		int bishopMaterial = Bitboards::countBits(pos.bitboards[Piece::B] | pos.bitboards[Piece::b]) * 320;
		int rookMaterial = Bitboards::countBits(pos.bitboards[Piece::R] | pos.bitboards[Piece::r]) * 500;
		int queenMaterial = Bitboards::countBits(pos.bitboards[Piece::Q] | pos.bitboards[Piece::q]) * 950;

		return ((pawnMaterial + knightMaterial + bishopMaterial + rookMaterial + queenMaterial) < 2600);
	}

	static bool hasNonPawnMaterial(Position& pos) {
		U64 whitePieces = pos.bitboards[Piece::N] | pos.bitboards[Piece::B] | pos.bitboards[Piece::R] | pos.bitboards[Piece::Q];
		U64 blackPieces = pos.bitboards[Piece::n] | pos.bitboards[Piece::b] | pos.bitboards[Piece::r] | pos.bitboards[Piece::q];

		return pos.sideToMove == Colors::white ? whitePieces != 0ULL : blackPieces != 0ULL;
	}

	static int contemptFactor(Position& pos) {
		if (isEndgame(pos))
			return 0;
		else
			return pos.sideToMove == Colors::white ? -Search::contempt : Search::contempt;
	}

	static U64 considerXrays(int sq, U64 occ, Position& pos) {
		U64 attackers = 0ULL;
		U64 attackingBishops = pos.bitboards[Piece::B] | pos.bitboards[Piece::b];
		U64 attackingRooks = pos.bitboards[Piece::R] | pos.bitboards[Piece::r];
		U64 attackingQueens = pos.bitboards[Piece::Q] | pos.bitboards[Piece::q];

		U64 intercardinalRays = Magic::getBishopAttacks(sq, occ);
		U64 cardinalRays = Magic::getRookAttacks(sq, occ);

		attackers |= intercardinalRays & (attackingBishops | attackingQueens);
		attackers |= cardinalRays & (attackingRooks | attackingQueens);

		return attackers;
	}

	static U64 minAttacker(U64 attadef, int sideToMove, int& attacker, Position& pos) {
		int startPiece = Piece::P;
		int endPiece = Piece::K;

		if (sideToMove == Colors::black) {
			startPiece = Piece::p;
			endPiece = Piece::k;
		}

		for (attacker = startPiece; attacker <= endPiece; attacker++) {
			U64 subset = attadef & pos.bitboards[attacker];
			if (subset) return (subset & (0 - subset));
		}

		return 0;
	}

	static int see(int move, Position& pos) {
		int gain[32];
		int idepth = 0;
		int sideToMove = pos.sideToMove ^ 1;

		int fromSq = getMoveSource(move);
		int toSq = getMoveTarget(move);
		int attacker = getMovePiece(move);
		bool isEnpassant = getMoveEnpassant(move) != 0;

		int startPiece = Piece::P;
		int endPiece = Piece::K;
		int target = -1;

		if (sideToMove == Colors::black) {
			startPiece = Piece::p;
			endPiece = Piece::k;
		}

		// En passant captures pawn that isnt on toSq
		int capturedSq = toSq;
		if (isEnpassant) {
			capturedSq = (fromSq & ~7) | (toSq & 7);
			target = startPiece;
		} else {
			for (int piece = startPiece; piece <= endPiece; piece++) {
				if (getBit(pos.bitboards[piece], toSq)) {
					target = piece;
					break;
				}
			}
		}

		if (target < 0) return 0;

		U64 seen = 0ULL;
		U64 occupied = pos.occupancies[Colors::both];
		if (isEnpassant) occupied &= ~(1ULL << capturedSq);
		U64 attackerBB = 1ULL << fromSq;

		U64 attadef = pos.attackersTo(toSq, occupied);
		U64 maxXray = occupied & ~(pos.bitboards[Piece::N] | pos.bitboards[Piece::K] | pos.bitboards[Piece::n] | pos.bitboards[Piece::k]);

		gain[idepth] = pieceValues[target];

		while (attackerBB) {
			idepth++;
			gain[idepth] = pieceValues[attacker] - gain[idepth - 1];

			if (std::max(-gain[idepth - 1], gain[idepth]) < 0) {
				break;
			}

			attadef &= ~attackerBB;
			occupied &= ~attackerBB;
			seen |= attackerBB;

			if ((attackerBB & maxXray) != 0) {
				attadef |= considerXrays(toSq, occupied, pos) & ~seen;
			}

			attackerBB = minAttacker(attadef, sideToMove, attacker, pos);
			sideToMove ^= 1;
		}

		for (idepth--; idepth > 0; idepth--) {
			gain[idepth - 1] = -std::max(-gain[idepth - 1], gain[idepth]);
		}

		return gain[0];
	}

	int scoreMove(int move, Position& pos, Threads::ThreadData* threadData) {
		// TT move gets highest priority
		if (threadData->scorePV && threadData->pvTable[0][threadData->ply] == move) {
			threadData->scorePV = false;
			return 30000;
		}
		
		if (getMoveCapture(move)) {
			int victim = Piece::P;
			int attacker = getMovePiece(move);
			U64 targetSquare = getMoveTarget(move);
			
			// Find victim piece
			for (int piece = Piece::P; piece <= Piece::k; piece++) {
				if (getBit(pos.bitboards[piece], targetSquare)) {
					victim = piece;
					break;
				}
			}
			
			// Improved capture scoring: victim_value - attacker_value/divisor + SEE
			int captureScore = pieceValues[victim] - pieceValues[attacker] / CaptureAttackerDivisor;
			int seeScore = see(move, pos);

			if (seeScore < 0) {
				return captureScore + seeScore;
			}

			return 10000 + captureScore + seeScore / CaptureSeeDivisor;
		}

		// Killer moves with ply-based aging
		if (threadData->killerMoves[0][threadData->ply] == move) {
			return 9000 - threadData->ply;
		}
		if (threadData->killerMoves[1][threadData->ply] == move) {
			return 8000 - threadData->ply;
		}

		int piece = getMovePiece(move);
		int target = getMoveTarget(move);
		int historyScore = threadData->historyMoves[piece][target];
		int contScore = continuationHistoryScore(piece, target, threadData);

		// Scale history score based on depth to maintain relevance
		return (historyScore + contScore) / (1 + threadData->ply / HistoryPlyDivisor);
	}

	void sortMoves(Movegen::MoveList* moveList, int bestMove, Position& pos, Threads::ThreadData* threadData) {
		int* moveScores = new int[moveList->count];

		for (int i = 0; i < moveList->count; i++) {
			if (bestMove == moveList->moves[i]) {
				moveScores[i] = 30000;
			} else {
				moveScores[i] = scoreMove(moveList->moves[i], pos, threadData);
			}
		}

		for (int i = 1; i < moveList->count; i++) {
			int currentMove = moveList->moves[i];
			int currentScore = moveScores[i];
			int j = i - 1;

			while (j >= 0 && moveScores[j] < currentScore) {
				moveList->moves[j + 1] = moveList->moves[j];
				moveScores[j + 1] = moveScores[j];
				j--;
			}

			moveList->moves[j + 1] = currentMove;
			moveScores[j + 1] = currentScore;
		}

		// Lazy SMP Root Move Rotation
		if (threadData->ply == 0 && threadData->threadId > 0 && moveList->count > 1) {
			int shift = threadData->threadId % moveList->count;

			std::vector<int> rotated(moveList->count);
			for (int i = 0; i < moveList->count; i++) {
				rotated[i] = moveList->moves[(i + shift) % moveList->count];
			}

			for (int i = 0; i < moveList->count; i++) {
				moveList->moves[i] = rotated[i];
			}
		}

		delete[] moveScores;
	}


	static int quiescence(int alpha, int beta, int depth, Position& pos, Threads::ThreadData* threadData) {
		// TODO: try to return 0 if position is draw

		if (threadData->ply > threadData->maxPly) threadData->maxPly = threadData->ply;

		bool pvNode = beta - alpha > 1;

		int bestMove    = 0;
		int ttEval      = EVAL_UNKNOWN;
		int ttFlag      = NO_HASH_ENTRY;
		int ttDepth     = 0;
		bool ttHit      = false;

		HASHE* ttEntry = readHashEntry(
			alpha,
			beta,
			&bestMove,
			&ttEval,
			&ttFlag,
			&ttDepth,
			depth,
			pos,
			&ttHit,
			threadData
		);

		// TODO: run mass test on this pvnode stuff.
		if (!pvNode && ttDepth >= 0 && ttEval != EVAL_UNKNOWN && ((ttFlag == hashfALPHA && ttEval <= alpha) || (ttFlag == hashfBETA && ttEval >= beta) || (ttFlag == hashfEXACT))) {
			return ttEval;
		}

		if ((threadData->nodes & 2047) == 0) pos.time.communicate();

		if (!Threads::tryVisitNode(threadData)) return 0;

		if (threadData->ply > MAX_PLY - 1) return Eval::evaluate(pos);

		int eval = Eval::evaluate(pos);

		if (eval >= beta) return beta;
		if (eval > alpha) alpha = eval;

		Movegen::MoveList moveList[1];
		Movegen::generateMoves(pos, moveList, true);
		sortMoves(moveList, 0, pos, threadData);

		for (int c = 0; c < moveList->count; c++) {
			int move = moveList->moves[c];

			if (see(move, pos) < 0) {
				continue;
			}

			copyBoard(pos);
			threadData->ply++;
			pos.repetitionIndex++;
			pos.repetitionTable[pos.repetitionIndex] = pos.hashKey;

			if (pos.makeMove(pos, move, captures) == 0) {
				threadData->ply--;
				pos.repetitionIndex--;
				continue;
			}

			int score = -quiescence(-beta, -alpha, depth, pos, threadData);
			threadData->ply--;
			pos.repetitionIndex--;
			takeBack(pos);

			if (pos.time.stopped == true || Threads::stopFlag) return 0;

			if (score > alpha) {
				alpha = score;

				if (score >= beta) {
					return beta;
				}
			}
		}

		return alpha;
	}

	void updateHistory(int move, int depth, bool good, Threads::ThreadData* td) {
		if (getMoveCapture(move)) return;

		int piece = getMovePiece(move);
		int target = getMoveTarget(move);

		int bonus = depth * depth;
		if (good) {
			td->historyMoves[piece][target] += bonus;
		} else {
			td->historyMoves[piece][target] -= bonus / HistoryMalusDivisor;
		}

		if (abs(td->historyMoves[piece][target]) > HistoryGravityThreshold) {
			for (int i = 0; i < 12; i++) {
				for (int j = 0; j < 64; j++) {
					td->historyMoves[i][j] /= 2;
				}
			}
		}
	}

	static int continuationHistoryScore(int piece, int target, Threads::ThreadData* td) {
		if (td->ply <= 0) return 0;

		int prevMove = td->ss[td->ply].currentMove;
		if (prevMove == 0) return 0;

		int prevPiece = getMovePiece(prevMove);
		int prevTarget = getMoveTarget(prevMove);

		return td->continuationHistory[Threads::contHistIndex(prevPiece, prevTarget, piece, target)];
	}

	static void updateContinuationHistory(int move, int depth, bool good, Threads::ThreadData* td) {
		if (getMoveCapture(move)) return;
		if (td->ply <= 0) return;

		int prevMove = td->ss[td->ply].currentMove;
		if (prevMove == 0) return;

		int prevPiece = getMovePiece(prevMove);
		int prevTarget = getMoveTarget(prevMove);
		int piece = getMovePiece(move);
		int target = getMoveTarget(move);

		int* entry = &td->continuationHistory[Threads::contHistIndex(prevPiece, prevTarget, piece, target)];
		int bonus = depth * depth;

		if (good) {
			*entry += bonus;
		} else {
			*entry -= bonus / HistoryMalusDivisor;
		}

		if (abs(*entry) > HistoryGravityThreshold) {
			for (int p = 0; p < 12; p++) {
				for (int t = 0; t < 64; t++) {
					td->continuationHistory[Threads::contHistIndex(prevPiece, prevTarget, p, t)] /= 2;
				}
			}
		}
	}

	static void updateQuietStats(int move, int depth, bool good, Threads::ThreadData* td) {
		updateHistory(move, depth, good, td);
		updateContinuationHistory(move, depth, good, td);
	}


// TODO: consider
int calculateReduction(int depth, int moveCount, bool pvNode, bool improving, 
                      int move, Position& pos, Threads::ThreadData* threadData) {
    
    if (moveCount < LmrMinMoveCount || depth < LmrMinDepth) return 0;
    if (getMoveCapture(move) || getMovePromotion(move)) return 0;
    if (move == threadData->killerMoves[0][threadData->ply] ||
        move == threadData->killerMoves[1][threadData->ply]) return 0;

    int R = std::max(1, (int)(LmrBase100 / 100.0 + log(depth) * log(moveCount) / (LmrDivisor100 / 100.0)));

    // Adjust based on node type
    if (pvNode) R = std::max(1, R - LmrPvReduction);

    // Reduce less if position is improving
    //if (!improving) R++;

    // History-based adjustments
    int piece = getMovePiece(move);
    int target = getMoveTarget(move);
    int historyScore = threadData->historyMoves[piece][target];

    // Reduce more for moves with bad history
    if (historyScore < -LmrHistoryThreshold) R++;
    if (historyScore > LmrHistoryThreshold) R = std::max(1, R - 1);

    R = std::min(R, depth - 1);

    return R;
}

	int Search::negamax(int alpha, int beta, int depth, bool cutnode, Position& pos, Threads::ThreadData* threadData) {
		if (threadData->ply > threadData->maxPly) threadData->maxPly = threadData->ply;

		SearchStack* currentSS = &threadData->ss[threadData->ply];
		
		threadData->pvLength[threadData->ply] = threadData->ply; // inits the PV length

		// less aggressive history/killer decay once time is getting short
		if (pos.time.getTimeMs() - pos.time.startTime < 5000)
			threadData->agingFactor = HistAgingLowTimePermille / 1000.0;

		int score = 0;
		//int bestMove = 0;
		int hashFlag = hashfALPHA;

		bool pvNode = beta - alpha > 1;
		bool isRoot = (threadData->ply == 0);

		if (threadData->ply && (isRepetition(pos, threadData) || pos.fifty >= 100)) return 0; // draw score, repetition has occured

		int bestMove    = 0;
		int ttEval      = EVAL_UNKNOWN;
		int ttFlag      = NO_HASH_ENTRY;
		int ttDepth     = 0;
		bool ttHit      = false;

		HASHE* ttEntry = readHashEntry(
			alpha,
			beta,
			&bestMove,
			&ttEval,
			&ttFlag,
			&ttDepth,
			depth,
			pos,
			&ttHit,
			threadData
		);

		if (!pvNode && ttDepth >= depth && ttEval != EVAL_UNKNOWN && ((ttFlag == hashfALPHA && ttEval <= alpha) || (ttFlag == hashfBETA && ttEval >= beta) || (ttFlag == hashfEXACT))) {
			return ttEval;
		}

		if ((threadData->nodes & 2047) == 0) pos.time.communicate();

		// age
		if ((threadData->nodes & 1000) == 0) {
			for (int i = 0; i < MAX_PLY; i++) {
				threadData->killerMoves[0][i] *= threadData->agingFactor;
				threadData->killerMoves[1][i] *= threadData->agingFactor;
			}

			for (int i = 0; i < 12; i++) {
				for (int j = 0; j < 64; j++) {
					threadData->historyMoves[i][j] *= threadData->agingFactor;
				}
			}
		}

		if (isRoot) {
			lastCurrmoveOutput = pos.time.startTime - CURRMOVE_INTERVAL;
		}

		// recursion escape condition
		if (depth == 0) return quiescence(alpha, beta, depth, pos, threadData);

		// preventing overflow of arrays
		if (threadData->ply > MAX_PLY - 1) return Eval::evaluate(pos);

		if (!Threads::tryVisitNode(threadData)) return 0;

		int kingCheck = pos.isSquareAttacked((pos.sideToMove == Colors::white) ? Bitboards::getLs1bIndex(pos.bitboards[Piece::K]) : Bitboards::getLs1bIndex(pos.bitboards[Piece::k]), pos.sideToMove ^ 1);
		if (kingCheck) depth++; // If the king is in check, then we increase Search::ply depth by one to prevent immediately getting mated

		int legalMoves = 0;
		int staticEval = Eval::evaluate(pos);
		
		currentSS->staticEval = staticEval;

		bool improving = false;

		if (threadData->ply >= 2) {
			if (threadData->ply >= 4 && threadData->ss[threadData->ply - 2].staticEval == EVAL_UNKNOWN) {
				improving = currentSS->staticEval > threadData->ss[threadData->ply - 4].staticEval || threadData->ss[threadData->ply - 4].staticEval == EVAL_UNKNOWN;
			}
			else {
				improving = currentSS->staticEval > threadData->ss[threadData->ply - 2].staticEval || threadData->ss[threadData->ply - 2].staticEval == EVAL_UNKNOWN;
			}
		}	

		if (threadData->ply && !pvNode && depth < 2 && (staticEval + RfpQMargin) <= alpha) return quiescence(alpha, beta, depth, pos, threadData);

		if (depth < 3 && !pvNode && !kingCheck && abs(beta - 1) > -VALUE_INFINITE + 100) {
			int evalMargin = RfpMargin1PerDepth * depth;

			if (staticEval - evalMargin >= beta) {
				return staticEval - evalMargin;
			}
		}

		// New beta pruning
		if (!pvNode && !kingCheck && depth <= 8 && staticEval - RfpMargin2PerDepth * std::max(0, (depth - improving)) >= beta) {
			return staticEval;
		}

		// TODO:
		/*// mate distance pruning
		int matingValue = MATE_VALUE - threadData->ply;
		if (matingValue < beta) {
			beta = matingValue;
			if (alpha >= matingValue) return matingValue;
		}
		int matingValueAlpha = -MATE_VALUE + threadData->ply;
		if (matingValueAlpha > alpha) {
			alpha = matingValueAlpha;
			if (beta <= matingValueAlpha) return matingValueAlpha;
		}*/

		// null move pruning
		if (depth >= NmpMinDepth && !kingCheck && threadData->ply && hasNonPawnMaterial(pos)) {
			copyBoard(pos);

			threadData->ply++;

			pos.repetitionIndex++;
			pos.repetitionTable[pos.repetitionIndex] = pos.hashKey;

			if (pos.enPassant != no_sq) // hash enpassant if available
				pos.hashKey ^= Zobrist::enPassantKeys[pos.enPassant];

			pos.enPassant = no_sq;
			pos.sideToMove ^= 1; // switching the side gives the opponent an extra move to make
			pos.hashKey ^= Zobrist::sideKey;

			int nmpReduction = NmpBaseReduction + (depth >= NmpDepthThreshold ? NmpDepthBonusReduction : 0);
			int nmpDepth = std::max(depth - 2 - nmpReduction, 0);
			score = -negamax(-beta, -beta + 1, nmpDepth, !cutnode, pos, threadData);

			threadData->ply--;
			pos.repetitionIndex--;

			takeBack(pos);

			if (pos.time.stopped == true || (Threads::stopFlag)) return 0; // returns 0 if time is up

			// fail hard beta cutoff
			if (score >= beta) {
				// store hash entry
				writeHashEntry(beta, bestMove, depth, hashfBETA, pos, threadData);

				return beta;
			}
		}

		bool canFutilityPrune = false;

		if (threadData->ply && !pvNode && !kingCheck && (depth <= FutilityMaxDepth)) {
			if ((staticEval + (FutilityMarginPerDepth * depth)) <= alpha) canFutilityPrune = true;
		}

		if (!pvNode && !kingCheck && depth <= RazorMaxDepth && threadData->ply > 0) {
			int razorMargin = RazorBaseMargin + RazorMarginPerDepth * depth;
			if (staticEval + razorMargin < alpha) {
				int razorScore = quiescence(alpha - razorMargin, alpha - razorMargin + 1, depth, pos, threadData);
				if (razorScore < alpha - razorMargin) {
					return razorScore;
				}
			}
		}


		// TODO: consider
		if (depth >= IirMinDepth && !ttHit && !(!(pvNode || cutnode)))
			depth--;

		// ProbCut
		int probCutBeta = beta + ProbCutMargin;

		if (depth >= ProbCutMinDepth && !pvNode && !kingCheck && threadData->ply > 0 && !(ttDepth >= depth - ProbCutTTDepthMargin && ttEval != EVAL_UNKNOWN && ttEval < probCutBeta)) {
			int reducedDepth = std::max(depth - ProbCutReduction, 0);

			Movegen::MoveList captureList[1];
			Movegen::generateMoves(pos, captureList, true);

			sortMoves(captureList, 0, pos, threadData);

			for (int c = 0; c < captureList->count; c++) {

				if (pos.time.stopped || Threads::stopFlag) return 0;

				if (see(captureList->moves[c], pos) < 0) {
					continue;
				}

				copyBoard(pos);

				threadData->ply++;

				pos.repetitionIndex++;
				pos.repetitionTable[pos.repetitionIndex] = pos.hashKey;

				if (pos.makeMove(pos, captureList->moves[c], allMoves) == 0) {
					threadData->ply--;

					pos.repetitionIndex--;

					continue; // skip to next move
				}

				score = -quiescence(-probCutBeta, -probCutBeta + 1, depth, pos, threadData);

				if (score >= probCutBeta) {
					score = -negamax(-probCutBeta, -probCutBeta + 1, reducedDepth, !cutnode, pos, threadData);
				}

				threadData->ply--;
				pos.repetitionIndex--;

				takeBack(pos);

				if (score >= probCutBeta) {
					writeHashEntry(score, captureList->moves[c], reducedDepth, hashfBETA, pos, threadData);

					return score;
				}
			}
		}

		Movegen::MoveList moveList[1];
		Movegen::generateMoves(pos, moveList, false);

		if (threadData->followPV) {
			enablePVScoring(moveList, threadData);
		}

		sortMoves(moveList, bestMove, pos, threadData); // sort the moves

		int movesSearched = 0;

		// Quiets tried at this node so far, in case one of them or a later one causes a beta cutoff
		int triedQuiets[256];
		int triedQuietsCount = 0;

		for (int c = 0; c < moveList->count; c++) {
			const int move = moveList->moves[c];

			copyBoard(pos);

			threadData->ply++;
			pos.repetitionIndex++;
			pos.repetitionTable[pos.repetitionIndex] = pos.hashKey;

			if (pos.makeMove(pos, moveList->moves[c], allMoves) == 0) { // make sure to only make the legal moves
				threadData->ply--;

				pos.repetitionIndex--;

				continue; // skip to next move
			}

			threadData->ss[threadData->ply].currentMove = move;

			if (!getMoveCapture(move) && triedQuietsCount < 256) {
				triedQuiets[triedQuietsCount++] = move;
			}

			if (isRoot && threadData->threadId == 0) {
				reportedCurrMove = false;

				int now = pos.time.getTimeMs();
				int elapsed = now - pos.time.startTime;
				int elapsedSinceLast = now - lastCurrmoveOutput;

				if (elapsed >= CURRMOVE_INITIAL_DELAY && elapsedSinceLast >= CURRMOVE_INTERVAL) {
                printf("info depth %d currmove %s currmovenumber %d\n",
                depth,
                Movegen::moveToString(move).c_str(),
                c + 1);

					lastCurrmoveOutput = now;
					reportedCurrMove = true;
				}
			}

			legalMoves++;

		if (movesSearched == 0) {
				score = -negamax(-beta, -alpha, depth - 1, !cutnode, pos, threadData);
			} 
			else {
				score = alpha + 1;
				bool skipMove = false;
				
				// **FUTILITY PRUNING**
				if (canFutilityPrune && (legalMoves > 1)) {
					if (!pos.isSquareAttacked(Bitboards::getLs1bIndex(pos.bitboards[(pos.sideToMove == Colors::white) ? Piece::K : Piece::k]), pos.sideToMove ^ 1)
						&& (threadData->killerMoves[0][threadData->ply] != move)
						&& (threadData->killerMoves[1][threadData->ply] != move)
						&& (getMovePiece(move) != Piece::P && getMovePiece(move) != Piece::p)
						&& !getMovePromotion(move)
						&& !getMoveCastling(move) 
						&& !getMoveCapture(move)) {
						
						skipMove = true;
					}
				}
				
				// **LATE MOVE PRUNING**
				if (!skipMove && threadData->ply && !pvNode && depth <= LmpMaxDepth && !kingCheck &&
					!getMoveCapture(move) && (legalMoves > LmpBase + LmpMult * depth * depth)) {
					skipMove = true;
				}

				// **HISTORY PRUNING**
				if (!skipMove && depth <= HistoryPruningMaxDepth && !getMoveCapture(move) && !getMovePromotion(move) &&
					move != threadData->killerMoves[0][threadData->ply] && move != threadData->killerMoves[1][threadData->ply]) {
					int historyScore = threadData->historyMoves[getMovePiece(move)][getMoveTarget(move)];
					if (historyScore < -HistoryPruningMargin * depth) {
						skipMove = true;
					}
				}
				
				if (skipMove) {
					pos.repetitionIndex--;
					threadData->ply--;
					takeBack(pos);
					continue;
				}
				
				// **LATE MOVE REDUCTION (LMR)**
				int reduction = calculateReduction(depth, movesSearched, pvNode, improving, move, pos, threadData);
				bool doLMR = false;
				
				if (movesSearched > 1 && depth >= LmrMinDepth && !kingCheck && !getMoveCapture(move)  && !getMovePromotion(move)) {
					
					doLMR = true;
				}

				
				// **PRINCIPAL VARIATION SEARCH (PVS)**
				if (doLMR && reduction > 0) {
					// First try reduced depth search with null window
					score = -negamax(-alpha - 1, -alpha, depth - 1 - reduction, true, pos, threadData);
					
					// If LMR search fails high, research at full depth with null window
					if (score > alpha) {
						score = -negamax(-alpha - 1, -alpha, depth - 1, false, pos, threadData);
					}
				} else {
					// No LMR, but still use null window for non-first moves
					score = -negamax(-alpha - 1, -alpha, depth - 1, false, pos, threadData);
				}
				
				// **FULL WINDOW RE-SEARCH**
				// If null window search fails high and we're in PV node, do full window search
				if (score > alpha && score < beta && pvNode) {
					score = -negamax(-beta, -alpha, depth - 1, !cutnode, pos, threadData);
				}
			}


			threadData->ply--;
			pos.repetitionIndex--;

			takeBack(pos);

			if (pos.time.stopped == true || Threads::stopFlag) return 0;

			movesSearched++;

			// if better move is found
			if (score > alpha) {
				// switch hash flag
				hashFlag = hashfEXACT;
				bestMove = move;

				if (!getMoveCapture(move)) {
					threadData->historyMoves[getMovePiece(move)][getMoveTarget(move)] += 1 << depth;
				}
				alpha = score; // PV node

				// TODO: see if only the main thread has to update PV
				//if (threadData->threadId == 0) {
					threadData->pvTable[threadData->ply][threadData->ply] = move;
	
					for (int next = threadData->ply + 1; next < threadData->pvLength[threadData->ply + 1]; next++) {
						threadData->pvTable[threadData->ply][next] = threadData->pvTable[threadData->ply + 1][next];
					}
	
					threadData->pvLength[threadData->ply] = threadData->pvLength[threadData->ply + 1];
				//}

				if (score >= beta) {
					writeHashEntry(beta, bestMove, depth, hashfBETA, pos, threadData);

					if (!getMoveCapture(move)) {
						// Update killer moves
						if (threadData->killerMoves[0][threadData->ply] != move) {
							threadData->killerMoves[1][threadData->ply] = threadData->killerMoves[0][threadData->ply];
							threadData->killerMoves[0][threadData->ply] = move;
						}

						// Reward move that caused cutoff, penalize all others tried at this node
						updateQuietStats(move, depth, true, threadData);

						for (int i = 0; i < triedQuietsCount; i++) {
							if (triedQuiets[i] == move) continue;
							updateQuietStats(triedQuiets[i], depth, false, threadData);
						}
					}

					return beta;
				}
			}
		}

		if (legalMoves == 0) {
			if (kingCheck) {
				return -MATE_VALUE + threadData->ply;
			}
			else {
				return contemptFactor(pos);
			}
		}

		writeHashEntry(alpha, bestMove, depth, hashFlag, pos, threadData);

		return alpha; // move fails low
	}

	void Search::iterativeDeepen(Threads::ThreadData* threadData) {
		threadData->score = 0;
		threadData->nodes = 0;
		threadData->agingFactor = HistAgingFactorPermille / 1000.0;

		threadData->followPV = false;
		threadData->scorePV = false;

		memset(threadData->killerMoves, 0, sizeof(threadData->killerMoves));
		memset(threadData->historyMoves, 0, sizeof(threadData->historyMoves));
		std::fill(threadData->continuationHistory.begin(), threadData->continuationHistory.end(), 0);
		memset(threadData->pvTable, 0, sizeof(threadData->pvTable));
		memset(threadData->pvLength, 0, sizeof(threadData->pvLength));
		memset(threadData->ss, 0, sizeof(threadData->ss));

		// Find legal root move instead of printing a8a8 if stopFlag is set before search loop
		int fallbackMove = 0;
		{
			Movegen::MoveList rootMoves[1];
			Movegen::generateMoves(threadData->pos, rootMoves, false);

			for (int c = 0; c < rootMoves->count; c++) {
				const int move = rootMoves->moves[c];
				copyBoard(threadData->pos);
				bool legal = threadData->pos.makeMove(threadData->pos, move, MoveType::allMoves) != 0;
				takeBack(threadData->pos);
				if (legal) {
					fallbackMove = move;
					break;
				}
			}
		}

		int alpha = -VALUE_INFINITE;
		int beta = VALUE_INFINITE;

		int confirmedPv[MAX_PLY];
		int confirmedPvLength = 0;
		bool haveConfirmed = false;

		// Iterative deepening loop
		for (int curDepth = 1; curDepth <= threadData->depth; curDepth++) {
			if (threadData->pos.time.stopped || Threads::stopFlag) break;

			if (threadData->threadId == 0) {
				threadData->followPV = true;
			}
			threadData->maxPly = 0;

			int delta = AspirationWindow;
			while (true) {
				threadData->score = Search::negamax(alpha, beta, curDepth, false, threadData->pos, threadData);

				if (threadData->pos.time.stopped || Threads::stopFlag) break;

				if (threadData->score <= alpha) {
					beta = (alpha + beta) / 2;
					alpha = std::max(threadData->score - delta, -VALUE_INFINITE);
					delta += delta / 2;
				} else if (threadData->score >= beta) {
					beta = std::min(threadData->score + delta, VALUE_INFINITE);
					delta += delta / 2;
				} else {
					break;
				}
			}

			if (threadData->pos.time.stopped || Threads::stopFlag) break;

			alpha = threadData->score - AspirationWindow;
			beta = threadData->score + AspirationWindow;

			// Main thread prints PV and total nodes
			if (threadData->threadId == 0 && threadData->pvLength[0]) {
				confirmedPvLength = threadData->pvLength[0];
				for (int c = 0; c < confirmedPvLength; c++) confirmedPv[c] = threadData->pvTable[0][c];
				haveConfirmed = true;

				U64 totalNodes = Threads::totalNodes.load(std::memory_order_relaxed);
				int time = threadData->pos.time.getTimeMs() - threadData->pos.time.startTime;
				if (time == 0) time = 1;
				U64 nps = static_cast<U64>(totalNodes * 1000) / time;
				int hashfull = hashFull();

				if (threadData->score > -MATE_VALUE && threadData->score < -MATE_SCORE) {
					printf("info depth %d seldepth %d score mate %d nodes %llu nps %llu hashfull %d time %d pv ",
						curDepth, threadData->maxPly, -(threadData->score + MATE_VALUE) / 2 - 1, totalNodes, nps, hashfull, time);
				}
				else if (threadData->score > MATE_SCORE && threadData->score < MATE_VALUE) {
					printf("info depth %d seldepth %d score mate %d nodes %llu nps %llu hashfull %d time %d pv ",
						curDepth, threadData->maxPly, (MATE_VALUE - threadData->score) / 2 + 1, totalNodes, nps, hashfull, time);
				}
				else {
					printf("info depth %d seldepth %d score cp %d nodes %llu nps %llu hashfull %d time %d pv ",
						curDepth, threadData->maxPly, threadData->score, totalNodes, nps, hashfull, time);
				}

				for (int c = 0; c < threadData->pvLength[0]; c++) {
					Movegen::printMove(threadData->pvTable[0][c]);
					printf(" ");
				}
				printf("\n");
			}
		}

		if (haveConfirmed) {
			for (int c = 0; c < confirmedPvLength; c++) threadData->pvTable[0][c] = confirmedPv[c];
			threadData->pvLength[0] = confirmedPvLength;
		} else if (fallbackMove != 0) {
			threadData->pvTable[0][0] = fallbackMove;
			threadData->pvLength[0] = 1;
		}
	}
}

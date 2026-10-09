#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <climits>
#include <new>

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

	size_t Search::hashBuckets = 0;
	HashBucket* Search::hashTable = NULL;
	uint8_t Search::hashGeneration = 0; // bumped once per search, wraps at 256

	int Search::contempt = 0;

	int lastCurrmoveOutput = 0;
	bool reportedCurrMove = false;
	const int CURRMOVE_INITIAL_DELAY = 2500;
	const int CURRMOVE_INTERVAL = 0;
	constexpr int HISTORY_MAX = 16384; // bound of each history table entry
	const int pieceValues[13] = { 100, 300, 300, 500, 900, VALUE_INFINITE, 100, 300, 300, 500, 900, VALUE_INFINITE, 0 };

	void Search::clearHashTable() {
		for (size_t b = 0; b < hashBuckets; b++) {
			for (int i = 0; i < TT_BUCKET_SIZE; i++) {
				hashTable[b].entries[i].keyXorData.store(0, std::memory_order_relaxed);
				hashTable[b].entries[i].data.store(0, std::memory_order_relaxed);
			}
		}
	}

	void Search::newSearchGeneration() {
		hashGeneration++;
	}

	// Packs bestMove[0..23] | depth[24..30] | flag[31..32] | score[33..55] | generation[56..63] into HASHE::data
	static inline uint64_t packHashData(int bestMove, int depth, int flag, int score, uint8_t generation) {
		// ProbCut can pass a negative depth, and the depth field is unsigned
		if (depth < 0) depth = 0;
		if (depth > 0x7F) depth = 0x7F; // 7-bit field, don't let a check-extended root depth wrap to 0
		uint64_t data = 0;
		data |= (uint64_t)((uint32_t)bestMove & 0xFFFFFFu);
		data |= (uint64_t)((uint32_t)depth & 0x7Fu) << 24;
		data |= (uint64_t)((uint32_t)flag & 0x3u) << 31;
		data |= (uint64_t)((uint32_t)score & 0x7FFFFFu) << 33;
		data |= (uint64_t)generation << 56;
		return data;
	}

	static inline void unpackHashData(uint64_t data, int* bestMove, int* depth, int* flag, int* score) {
		*bestMove = (int)(data & 0xFFFFFFu);
		*depth = (int)((data >> 24) & 0x7Fu);
		*flag = (int)((data >> 31) & 0x3u);
		uint32_t rawScore = (uint32_t)((data >> 33) & 0x7FFFFFu);
		if (rawScore & 0x400000u) rawScore |= 0xFF800000u; // sign-extend 23-bit field
		*score = (int)rawScore;
	}

	static inline uint8_t hashDataGeneration(uint64_t data) {
		return (uint8_t)(data >> 56);
	}

	static inline HashBucket* hashBucketFor(U64 hashKey) {
		// Multiply-high maps the key onto [0, hashBuckets) without a 64-bit division
		return &Search::hashTable[(size_t)(((unsigned __int128)hashKey * Search::hashBuckets) >> 64)];
	}

	void Search::freeHashTable() {
		delete[] hashTable;
		hashTable = NULL;
		hashBuckets = 0;
	}

	void Search::initHashTable(int mb) {
		if (hashTable != NULL) {
			printf("info string Clearing hash memory\n");
			freeHashTable();
		}

		// size_t math, an int byte count overflows from 2048 MB up
		size_t hashSize = (size_t)mb * 0x100000;
		hashBuckets = hashSize / sizeof(HashBucket);

		hashTable = new (std::nothrow) HashBucket[hashBuckets];

		if (hashTable == NULL) {
			hashBuckets = 0;
			if (mb <= 1) {
				printf("info string Couldnt allocate memory for hash table\n");
				exit(EXIT_FAILURE);
			}
			printf("info string Couldnt allocate memory for hash table, trying %dMB\n", mb / 2);
			initHashTable(mb / 2);
		} else {
			clearHashTable();
			printf("info string Hash table is initialized with %llu entries\n", (unsigned long long)(hashBuckets * TT_BUCKET_SIZE));
		}
	}

	static HASHE* readHashEntry(int alpha, int beta, int* bestMove, int* ttEval, int* ttFlag, int* ttDepth, int depth, Position& pos, bool* hit, Threads::ThreadData* threadData) {

		HashBucket* bucket = hashBucketFor(pos.hashKey);
		*hit = false;

		for (int i = 0; i < TT_BUCKET_SIZE; i++) {
			HASHE* hashEntry = &bucket->entries[i];

			// Lockless hashing, torn combination of old/new fails reconstruction
			uint64_t data = hashEntry->data.load(std::memory_order_relaxed);
			uint64_t storedKeyXor = hashEntry->keyXorData.load(std::memory_order_relaxed);

			if ((storedKeyXor ^ data) != pos.hashKey) continue;

			// Still useful in this search, so stamp it with the current generation to keep it from aging out
			if (hashDataGeneration(data) != Search::hashGeneration) {
				data = (data & 0x00FFFFFFFFFFFFFFull) | ((uint64_t)Search::hashGeneration << 56);
				hashEntry->data.store(data, std::memory_order_relaxed);
				hashEntry->keyXorData.store(pos.hashKey ^ data, std::memory_order_relaxed);
			}

			int storedBestMove, storedDepth, storedFlag, storedScore;
			unpackHashData(data, &storedBestMove, &storedDepth, &storedFlag, &storedScore);

			if (storedDepth >= depth) {
				*bestMove = storedBestMove;

				int score = storedScore;
				if (score < -MATE_SCORE) score += threadData->ply;
				if (score > MATE_SCORE) score -= threadData->ply;

				*ttEval = score;
				*ttFlag = storedFlag;
				*ttDepth = storedDepth;

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

		return nullptr;
	}

	static void writeHashEntry(int score, int bestMove, int depth, int hashFlag, Position& pos, Threads::ThreadData* threadData) {
		HashBucket* bucket = hashBucketFor(pos.hashKey);
		const uint8_t generation = Search::hashGeneration;

		int adjustedScore = score;
		if (adjustedScore < -MATE_SCORE) adjustedScore -= threadData->ply;
		if (adjustedScore > MATE_SCORE) adjustedScore += threadData->ply;

		HASHE* replace = &bucket->entries[0];
		uint64_t replaceData = 0;
		bool sameKey = false;
		int worstValue = INT_MAX;

		for (int i = 0; i < TT_BUCKET_SIZE; i++) {
			HASHE* hashEntry = &bucket->entries[i];
			uint64_t data = hashEntry->data.load(std::memory_order_relaxed);
			uint64_t storedKeyXor = hashEntry->keyXorData.load(std::memory_order_relaxed);

			if ((storedKeyXor ^ data) == pos.hashKey) {
				replace = hashEntry;
				replaceData = data;
				sameKey = true;
				break;
			}

			int value;
			if (data == 0 && storedKeyXor == 0) {
				value = INT_MIN;
			} else {
				int storedDepth = (int)((data >> 24) & 0x7Fu);
				int age = (uint8_t)(generation - hashDataGeneration(data));
				value = storedDepth - 8 * age;
			}

			if (value < worstValue) {
				worstValue = value;
				replace = hashEntry;
			}
		}

		if (sameKey) {
			int oldMove, oldDepth, oldFlag, oldScore;
			unpackHashData(replaceData, &oldMove, &oldDepth, &oldFlag, &oldScore);

			bool stale = hashDataGeneration(replaceData) != generation;
			if (hashFlag != hashfEXACT && !stale && depth + 4 <= oldDepth) return;

			// A fail-low has no best move of its own, keep the old one for move ordering
			if (bestMove == 0) bestMove = oldMove;
		}

		uint64_t newData = packHashData(bestMove, depth, hashFlag, adjustedScore, generation);
		replace->data.store(newData, std::memory_order_relaxed);
		replace->keyXorData.store(pos.hashKey ^ newData, std::memory_order_relaxed);
	}

	int hashFull() {
		// Sample a portion of the hash table to estimate usage
		size_t sampleBuckets = std::min((size_t)(1000 / TT_BUCKET_SIZE), Search::hashBuckets);
		int used = 0;

		for (size_t b = 0; b < sampleBuckets; b++) {
			for (int i = 0; i < TT_BUCKET_SIZE; i++) {
				uint64_t data = Search::hashTable[b].entries[i].data.load(std::memory_order_relaxed);
				uint64_t storedKeyXor = Search::hashTable[b].entries[i].keyXorData.load(std::memory_order_relaxed);
				if ((data != 0 || storedKeyXor != 0) && hashDataGeneration(data) == Search::hashGeneration) {
					used++;
				}
			}
		}

		return (int)(1000 * used / (sampleBuckets * TT_BUCKET_SIZE));
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

	static bool hasNonPawnMaterial(Position& pos) {
		U64 whitePieces = pos.bitboards[Piece::N] | pos.bitboards[Piece::B] | pos.bitboards[Piece::R] | pos.bitboards[Piece::Q];
		U64 blackPieces = pos.bitboards[Piece::n] | pos.bitboards[Piece::b] | pos.bitboards[Piece::r] | pos.bitboards[Piece::q];

		return pos.sideToMove == Colors::white ? whitePieces != 0ULL : blackPieces != 0ULL;
	}

	static int drawScore(Position& pos, Threads::ThreadData* threadData) {
		return pos.sideToMove == threadData->rootSide ? -Search::contempt : Search::contempt;
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

	// Quiets land between killers and bad captures
	constexpr int PV_MOVE_SCORE      =  2000000;
	constexpr int GOOD_CAPTURE_SCORE =  1000000;
	constexpr int KILLER1_SCORE      =   900000;
	constexpr int KILLER2_SCORE      =   800000;
	constexpr int BAD_CAPTURE_SCORE  = -1000000;

	int scoreMove(int move, Position& pos, Threads::ThreadData* threadData) {
		// TT move gets highest priority
		if (threadData->scorePV && threadData->pvTable[0][threadData->ply] == move) {
			threadData->scorePV = false;
			return PV_MOVE_SCORE;
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
				return BAD_CAPTURE_SCORE + captureScore + seeScore;
			}

			return GOOD_CAPTURE_SCORE + captureScore + seeScore / CaptureSeeDivisor;
		}

		// Killer moves with ply-based aging
		if (threadData->killerMoves[0][threadData->ply] == move) {
			return KILLER1_SCORE - threadData->ply;
		}
		if (threadData->killerMoves[1][threadData->ply] == move) {
			return KILLER2_SCORE - threadData->ply;
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
				moveScores[i] = PV_MOVE_SCORE;
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

		if (!pvNode && ttDepth >= 0 && ttEval != EVAL_UNKNOWN && ((ttFlag == hashfALPHA && ttEval <= alpha) || (ttFlag == hashfBETA && ttEval >= beta) || (ttFlag == hashfEXACT))) {
			return ttEval;
		}

		if ((threadData->nodes & 2047) == 0) pos.time.communicate();

		if (!Threads::tryVisitNode(threadData)) return 0;

		if (threadData->ply > MAX_PLY - 1) return Eval::evaluate(pos);

		int kingSq = Bitboards::getLs1bIndex(pos.bitboards[(pos.sideToMove == Colors::white) ? Piece::K : Piece::k]);
		bool inCheck = pos.isSquareAttacked(kingSq, pos.sideToMove ^ 1);

		// Standing pat is illegal in check: every evasion is searched, and having none is mate
		int eval = -MATE_VALUE + threadData->ply;

		if (!inCheck) {
			eval = Eval::evaluate(pos);

			if (eval >= beta) return beta;
			if (eval > alpha) alpha = eval;
		}

		Movegen::MoveList moveList[1];
		Movegen::generateMoves(pos, moveList, !inCheck);
		sortMoves(moveList, 0, pos, threadData);

		int legalMoves = 0;

		for (int c = 0; c < moveList->count; c++) {
			int move = moveList->moves[c];

			if (!inCheck && see(move, pos) < 0) {
				continue;
			}

			copyBoard(pos);
			threadData->ply++;
			pos.repetitionIndex++;
			pos.repetitionTable[pos.repetitionIndex] = pos.hashKey;

			if (pos.makeMove(pos, move, inCheck ? allMoves : captures) == 0) {
				threadData->ply--;
				pos.repetitionIndex--;
				continue;
			}

			legalMoves++;

			int score = -quiescence(-beta, -alpha, depth, pos, threadData);
			threadData->ply--;
			pos.repetitionIndex--;
			takeBack(pos);

			if (pos.time.stopped == true || Threads::stopFlag) return 0;

			if (score > eval) {
				bestMove = move;

				if (score > alpha) {
					alpha = score;

					// pv update
					threadData->pvTable[threadData->ply][threadData->ply] = move;
					threadData->pvLength[threadData->ply] = threadData->ply + 1;
				}

				if (score >= beta) {
					return beta;
				}
			}
		}

		if (inCheck && legalMoves == 0) return -MATE_VALUE + threadData->ply;

		return alpha;
	}

	// Gravity update
	static void applyHistoryBonus(int* entry, int bonus) {
		bonus = clamp(bonus, -HISTORY_MAX, HISTORY_MAX);
		*entry += bonus - *entry * abs(bonus) / HISTORY_MAX;
	}

	static int historyBonus(int depth, bool good) {
		int bonus = clamp(HistBonusMul * depth - HistBonusSub, 0, HistBonusMax);
		return good ? bonus : -bonus / HistoryMalusDivisor;
	}

	void updateHistory(int move, int depth, bool good, Threads::ThreadData* td) {
		if (getMoveCapture(move)) return;

		applyHistoryBonus(&td->historyMoves[getMovePiece(move)][getMoveTarget(move)], historyBonus(depth, good));
	}

	static int continuationHistoryScore(int piece, int target, Threads::ThreadData* td) {
		if (td->ply <= 0) return 0;

		int prevMove = td->ss[td->ply].currentMove;
		if (prevMove == 0) return 0;

		int prevPiece = getMovePiece(prevMove);
		int prevTarget = getMoveTarget(prevMove);

		return td->continuationHistory[Threads::contHistIndex(prevPiece, prevTarget, piece, target)];
	}

	static int madeQuietHistory(int move, Threads::ThreadData* td) {
		int piece = getMovePiece(move);
		int target = getMoveTarget(move);
		int score = td->historyMoves[piece][target];

		int prevMove = td->ss[td->ply - 1].currentMove;
		if (prevMove) score += td->continuationHistory[Threads::contHistIndex(getMovePiece(prevMove), getMoveTarget(prevMove), piece, target)];

		return score;
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

		applyHistoryBonus(&td->continuationHistory[Threads::contHistIndex(prevPiece, prevTarget, piece, target)], historyBonus(depth, good));
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
    // called after the move is made, so this node's killers are one ply up
    if (move == threadData->killerMoves[0][threadData->ply - 1] ||
        move == threadData->killerMoves[1][threadData->ply - 1]) return 0;

    int R = std::max(1, (int)(LmrBase100 / 100.0 + log(depth) * log(moveCount) / (LmrDivisor100 / 100.0)));

    // Adjust based on node type
    if (pvNode) R = std::max(1, R - LmrPvReduction);

    // Reduce good-history moves less, bad-history moves more
    R = std::max(1, R - madeQuietHistory(move, threadData) / LmrHistoryDivisor);

    R = std::min(R, depth - 1);

    return R;
}

	int Search::negamax(int alpha, int beta, int depth, bool cutnode, Position& pos, Threads::ThreadData* threadData) {

		if (threadData->ply > threadData->maxPly) threadData->maxPly = threadData->ply;

		SearchStack* currentSS = &threadData->ss[threadData->ply];
		
		threadData->pvLength[threadData->ply] = threadData->ply; // inits the PV length

		int score = 0;
		int hashFlag = hashfALPHA;

		bool pvNode = beta - alpha > 1;
		bool isRoot = (threadData->ply == 0);

		if (threadData->ply && (isRepetition(pos, threadData) || pos.fifty >= 100)) return drawScore(pos, threadData); // repetition or fifty-move draw

		// Mate distance pruning
		if (!isRoot) {
			alpha = std::max(alpha, -MATE_VALUE + threadData->ply);
			beta = std::min(beta, MATE_VALUE - threadData->ply - 1);
			if (alpha >= beta) return alpha;
		}

		int kingCheck = pos.isSquareAttacked((pos.sideToMove == Colors::white) ? Bitboards::getLs1bIndex(pos.bitboards[Piece::K]) : Bitboards::getLs1bIndex(pos.bitboards[Piece::k]), pos.sideToMove ^ 1);
		if (kingCheck) depth++;

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

		if (isRoot) {
			lastCurrmoveOutput = pos.time.startTime - CURRMOVE_INTERVAL;
		}

		// recursion escape condition
		if (depth == 0) return quiescence(alpha, beta, depth, pos, threadData);

		// preventing overflow of arrays
		if (threadData->ply > MAX_PLY - 1) return Eval::evaluate(pos);

		if (!Threads::tryVisitNode(threadData)) return 0;

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

		// null move pruning
		if (depth >= NmpMinDepth && !kingCheck && threadData->ply && hasNonPawnMaterial(pos) && abs(beta) < MATE_SCORE) {
			copyBoard(pos);

			threadData->ply++;
			threadData->ss[threadData->ply].currentMove = 0; // no previous move for continuation history

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

		// ProbCut
		int probCutBeta = beta + ProbCutMargin;

		if (depth >= ProbCutMinDepth && !pvNode && !kingCheck && threadData->ply > 0 && abs(beta) < MATE_SCORE && !(ttDepth >= depth - ProbCutTTDepthMargin && ttEval != EVAL_UNKNOWN && ttEval < probCutBeta)) {
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

				threadData->ss[threadData->ply].currentMove = captureList->moves[c];

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

		// Quiets searched at this node so far, in case one of them or a later one causes a beta cutoff
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

			bool givesCheck = pos.isSquareAttacked(Bitboards::getLs1bIndex(pos.bitboards[(pos.sideToMove == Colors::white) ? Piece::K : Piece::k]), pos.sideToMove ^ 1);

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
					if (!givesCheck
						&& (threadData->killerMoves[0][threadData->ply - 1] != move)
						&& (threadData->killerMoves[1][threadData->ply - 1] != move)
						&& (getMovePiece(move) != Piece::P && getMovePiece(move) != Piece::p)
						&& !getMovePromotion(move)
						&& !getMoveCastling(move) 
						&& !getMoveCapture(move)) {
						
						skipMove = true;
					}
				}
				
				// **LATE MOVE PRUNING**
				if (!skipMove && threadData->ply && !pvNode && depth <= LmpMaxDepth && !kingCheck && !givesCheck &&
					!getMoveCapture(move) && (legalMoves > LmpBase + LmpMult * depth * depth)) {
					skipMove = true;
				}

				// **HISTORY PRUNING**
				if (!skipMove && depth <= HistoryPruningMaxDepth && !givesCheck && !getMoveCapture(move) && !getMovePromotion(move) &&
					move != threadData->killerMoves[0][threadData->ply - 1] && move != threadData->killerMoves[1][threadData->ply - 1]) {
					if (madeQuietHistory(move, threadData) < -HistoryPruningMargin * depth) {
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
				if (givesCheck) reduction = std::max(0, reduction - 1);
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

			// Searched quiets are penalized on cutoff, penalzing pruned ones would push history down
			if (!getMoveCapture(move) && triedQuietsCount < 256) {
				triedQuiets[triedQuietsCount++] = move;
			}

			// if better move is found
			if (score > alpha) {
				// switch hash flag
				hashFlag = hashfEXACT;
				bestMove = move;

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
				return drawScore(pos, threadData); // stalemate
			}
		}

		writeHashEntry(alpha, bestMove, depth, hashFlag, pos, threadData);

		return alpha; // move fails low
	}

	void Search::iterativeDeepen(Threads::ThreadData* threadData) {
		threadData->score = 0;
		threadData->nodes = 0;

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

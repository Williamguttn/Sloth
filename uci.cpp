#define NOMINMAX
#include <iostream>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>
#include <stdlib.h>

#include "uci.h"

#include "movegen.h"
#include "piece.h"
#include "position.h"
#include "movegen.h"
#include "search.h"
#include "perft.h"

namespace Sloth {
	int UCI::parseMove(Position& pos, const char* moveString) {
		const char* squareToCoordinates[] = { // temporary
				"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
				"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
				"a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
				"a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
				"a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
				"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
				"a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
				"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
		};

		// MIGHT BE A FASTER AND BETTER WAY OF DOING THIS (like without generating moves all over again)
		Movegen::MoveList moveList[1];

		Movegen::generateMoves(pos, moveList, false);

		int sourceSquare = (moveString[0] - 'a') + (8 - (moveString[1] - '0')) * 8;
		int targetSquare = (moveString[2] - 'a') + (8 - (moveString[3] - '0')) * 8;

		//printf("source square: %s Target square: %s\n", squareToCoordinates[sourceSquare], squareToCoordinates[targetSquare]);

		// this part especially might be slow
		for (int c = 0; c < moveList->count; c++) {
			int move = moveList->moves[c];

			if (sourceSquare == getMoveSource(move) && targetSquare == getMoveTarget(move)) {
				int promoted = getMovePromotion(move);

				if (promoted) {
					char pIndex = moveString[4];

					// remake this in the future
					if ((promoted == Piece::Q || promoted == Piece::q) && pIndex == 'q') {
						return move;
					}
					else if ((promoted == Piece::R || promoted == Piece::r) && pIndex == 'r') {
						return move;
					}
					else if ((promoted == Piece::B || promoted == Piece::b) && pIndex == 'b') {
						return move;
					}
					else if ((promoted == Piece::N || promoted == Piece::n) && pIndex == 'n') {
						return move;
					}

					continue;
				}

				return move; // legal move
			}
		}

		return 0;
	}

	void UCI::parsePosition(Position& pos, const char* command) {
		command += 9; // shift pointer past "position" (Like in "position startpos")

		// allocate memory for copy of const char* command
		char* cmdCpy = new char[strlen(command) + 1];
		strcpy_s(cmdCpy, sizeof(char) * (strlen(command) + 1), command);
		//char* curChar = strstr(cmdCpy, "fen");
		char* curChar = cmdCpy;

		if (strncmp(command, "startpos", 8) == 0) { // startpos found
			// init chess board with start pos
			pos.parseFen(startPosition);
		}
		else {
			// parse fen command

			curChar = strstr(cmdCpy, "fen");

			if (curChar == NULL) {
				pos.parseFen(startPosition);
			}
			else {
				// substring found
				curChar += 4;

				pos.parseFen(curChar); // loads the fen string in "position fen xxxxxxxxxxxxxxxxx"
			}
		}

		curChar = strstr(cmdCpy, "moves");

		if (curChar != NULL) {
			curChar += 6;

			while (*curChar) { // loops over moves withing move string
				int move = parseMove(pos, curChar);

				if (move == 0) break;

				Search::repetitionIndex++;
				Search::repetitionTable[Search::repetitionIndex] = pos.hashKey;

				pos.makeMove(pos, move, MoveType::allMoves);

				while (*curChar && *curChar != ' ') curChar++; // moves cur char pointer to end of cur move

				// go to next move
				curChar++;
			}

		}

		// free the allocated memory
		delete[] cmdCpy;
	}

	void resetTimeControl(Position& pos) {
		pos.time.quit = false;
		pos.time.movesToGo = 30;
		pos.time.moveTime = -1;
		pos.time.time = -1;
		pos.time.inc = 0;
		pos.time.startTime = 0;
		pos.time.stopTime = 0;
		pos.time.timeSet = 0;
		pos.time.stopped = 0;
	}

	void UCI::parseGo(Position& pos, const char* command) {
		resetTimeControl(pos);

		int depth = -1;
		bool perft = false;

		char* cmdCpy = new char[strlen(command) + 1];
		strcpy_s(cmdCpy, sizeof(char) * (strlen(command) + 1), command);

		char* curDepth = NULL;

		char* argument = NULL;

		if ((argument = strstr(cmdCpy, "infinite"))) {}

		if ((argument = strstr(cmdCpy, "binc")) && pos.sideToMove == Colors::black)
			pos.time.inc = atoi(argument + 5);

		if ((argument = strstr(cmdCpy, "winc")) && pos.sideToMove == Colors::white)
			pos.time.inc = atoi(argument + 5);

		if ((argument = strstr(cmdCpy, "wtime")) && pos.sideToMove == Colors::white)
			pos.time.time = atoi(argument + 6);

		if ((argument = strstr(cmdCpy, "btime")) && pos.sideToMove == Colors::black)
			pos.time.time = atoi(argument + 6);

		if (argument = strstr(cmdCpy, "movestogo"))
			pos.time.movesToGo = atoi(argument + 10);

		if (argument = strstr(cmdCpy, "movetime"))
			pos.time.moveTime = atoi(argument + 9);

		if (argument = strstr(cmdCpy, "depth"))
			depth = atoi(argument + 6);

		if (argument = strstr(cmdCpy, "perft")) {
			depth = atoi(argument + 6);
			perft = true;
		}

		if (!perft) {
			if (pos.time.moveTime != -1) {
				pos.time.time = pos.time.moveTime;

				pos.time.movesToGo = 1;
			}

			pos.time.startTime = pos.time.getTimeMs();

			if (pos.time.time != -1) {
				pos.time.timeSet = 1;

				pos.time.time /= pos.time.movesToGo;
				if (pos.time.time > 1500) pos.time.time -= 50;
				pos.time.stopTime = pos.time.startTime + pos.time.time + pos.time.inc;

				if (pos.time.time < 1500 && pos.time.inc && depth == 64) pos.time.stopTime = pos.time.startTime + pos.time.inc - 50;
			}

			if (depth == -1) { // if depth is unavailable then set to 64
				depth = 64;
			}

			//printf("time:%d start:%d stop:%d depth:%d timeset:%d\n", pos.time.time, pos.time.startTime, pos.time.stopTime, depth, pos.time.timeSet);

			Search::search(pos, depth);
		}
		else {
			if (Bitboards::occupancies[Colors::both] == 0ULL)
				parsePosition(game, "position startpos");

			Perft::perftTest(depth, pos);
		}

		delete[] cmdCpy;
	}

	void UCI::loop() {
		// reset STDIN and STDOUT buffers
		setvbuf(stdin, NULL, _IONBF, 0);
		setvbuf(stdout, NULL, _IONBF, 0);

		char input[2000];

		int mbHash = 0;

		printf("Sloth version %s\n", VERSION);

		while (true) {
			memset(input, 0, sizeof(input));
			fflush(stdout);

			if (!fgets(input, 2000, stdin)) continue;

			if (input[0] == '\n') continue;

			if (strncmp(input, "isready", 7) == 0) {
				printf("readyok\n");

				continue;
			}
			else if (strncmp(input, "position", 8) == 0) {
				parsePosition(game, input);

				Search::clearHashTable();
			}
			else if (strncmp(input, "ucinewgame", 10) == 0) {
				//game.parseFen(startPosition); // THIS IS TEMPORARY UNTIL PROBLEM IS FIXED

				parsePosition(game, "position startpos");

				Search::clearHashTable();
			}
			else if (strncmp(input, "go", 2) == 0) {
				parseGo(game, input);
			}
			else if (strncmp(input, "quit", 4) == 0) {
				break;
			}
			else if (strncmp(input, "uci", 3) == 0) {
				printf("id name Sloth %s\n", VERSION);
				printf("id author William Sjolund\n");
				printf("option name Hash type spin default 64 min %d max %d\n", MIN_HASH, MAX_HASH);
				printf("option name Contempt type spin default 0 min 0 max 200\n");

				printf("uciok\n");
			}
			else if (!strncmp(input, "setoption name Hash value ", 26)) {
				sscanf_s(input, "%*s %*s %*s %*s %d", &mbHash);

				if (mbHash < MIN_HASH) mbHash = MIN_HASH;
				if (mbHash > MAX_HASH) mbHash = MAX_HASH;

				Search::initHashTable(mbHash);
			}
			else if (!strncmp(input, "setoption name Contempt value ", 30)) {
				int contempt;

				sscanf_s(input, "%*s %*s %*s %*s %d", &contempt);

				if (contempt < 0) contempt = 0;
				if (contempt > 200) contempt = 200;

				Search::contempt = contempt;
			}
		}
	}
}
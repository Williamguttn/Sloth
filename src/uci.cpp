#undef NOMINMAX
#define NOMINMAX 1

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
#include "search.h"
#include "threads.h"
#include "perft.h"
#include "evaluate.h"
#include "tune.h"

#ifndef _WIN32
#include <cstdio>
#include <cstdarg>

static int sscanf_s(const char *buffer, const char *format, ...) {       // for GCC/Clang - JA
    va_list args;
    va_start(args, format);
    int result = vsscanf(buffer, format, args);
    va_end(args);
    return result;
}

#include <cstring>
#include <stdexcept>

static void strcpy_s(char* dest, size_t destsz, const char* src) {        // for GCC/Clang - JA
    if (dest == nullptr || src == nullptr) {
        throw std::invalid_argument("Null pointer argument");
    }
    size_t src_len = std::strlen(src);
    if (src_len >= destsz) {
        throw std::length_error("Destination buffer too small");
    }
    std::strcpy(dest, src);
}

#endif

namespace Sloth {
    int UCI::parseMove(Position& pos, const char* moveString) {
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

        Movegen::MoveList moveList[1];
        Movegen::generateMoves(pos, moveList, false);

        int sourceSquare = (moveString[0] - 'a') + (8 - (moveString[1] - '0')) * 8;
        int targetSquare = (moveString[2] - 'a') + (8 - (moveString[3] - '0')) * 8;

        for (int c = 0; c < moveList->count; c++) {
            int move = moveList->moves[c];

            if (sourceSquare == getMoveSource(move) && targetSquare == getMoveTarget(move)) {
                int promoted = getMovePromotion(move);

                if (promoted) {
                    char pIndex = moveString[4];

                    if ((promoted == Piece::Q || promoted == Piece::q) && pIndex == 'q') {
                        return move;
                    } else if ((promoted == Piece::R || promoted == Piece::r) && pIndex == 'r') {
                        return move;
                    } else if ((promoted == Piece::B || promoted == Piece::b) && pIndex == 'b') {
                        return move;
                    } else if ((promoted == Piece::N || promoted == Piece::n) && pIndex == 'n') {
                        return move;
                    }

                    continue;
                }

                return move;
            }
        }

        return 0;
    }

    void UCI::parsePosition(Position& pos, const char* command) {
        command += 9;

        char* cmdCpy = new char[strlen(command) + 1];
        strcpy_s(cmdCpy, strlen(command) + 1, command);
        char* curChar = cmdCpy;

        if (strncmp(command, "startpos", 8) == 0) {
            pos.parseFen(startPosition);
        } else {
            curChar = strstr(cmdCpy, "fen");

            if (curChar == NULL) {
                pos.parseFen(startPosition);
            } else {
                curChar += 4;
                pos.parseFen(curChar);
            }
        }

        curChar = strstr(cmdCpy, "moves");

        int lastMove = 0;

        if (curChar != NULL) {
            curChar += 6;

            int moveNumber = 0;

            while (*curChar) {
                int move = parseMove(pos, curChar);

                if (move == 0) {
                    // Token matched no legal move; stop replaying and warn instead of silently desyncing.
                    char preview[16] = { 0 };
                    int previewLen = 0;
                    while (curChar[previewLen] && curChar[previewLen] != ' ' && previewLen < 15) {
                        preview[previewLen] = curChar[previewLen];
                        previewLen++;
                    }
                    if (previewLen > 0) {
                        printf("info string DESYNC: move token '%s' (token #%d in moves list) matched no legal move -- replay stopped here, rest of the moves list was NOT applied\n",
                            preview, moveNumber + 1);
                        fflush(stdout);
                    }
                    break;
                }

                pos.repetitionIndex++;
                pos.repetitionTable[pos.repetitionIndex] = pos.hashKey;

                pos.makeMove(pos, move, MoveType::allMoves);
                lastMove = move;
                moveNumber++;

                U64 recomputedKey = Zobrist::generateHashKey(pos);
                if (recomputedKey != pos.hashKey) {
                    printf("info string DESYNC: hash mismatch after move %s (token #%d in moves list) -- incremental=%llx recomputed=%llx sideToMove=%d castle=%d enpassant=%d\n",
                        Movegen::moveToString(move).c_str(), moveNumber,
                        (unsigned long long)pos.hashKey, (unsigned long long)recomputedKey,
                        pos.sideToMove, pos.castle, pos.enPassant);
                    fflush(stdout);
                }

                // Skip the token and any trailing whitespace
                while (*curChar && *curChar != ' ' && *curChar != '\n' && *curChar != '\r' && *curChar != '\t') curChar++;
                while (*curChar == ' ' || *curChar == '\n' || *curChar == '\r' || *curChar == '\t') curChar++;
            }
        }

        //Search::clearHashTable();ddd

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
        pos.time.ponder = false;
    }
    
    int numThreads = 1;

    void UCI::parseGo(Position& pos, const char* command) {
        resetTimeControl(pos);
        bool ponder = false;

        int depth = -1;
        U64 nodes = 0;
        bool perft = false;

        char* cmdCpy = new char[strlen(command) + 1];
        strcpy_s(cmdCpy, strlen(command) + 1, command);

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

        if ((argument = strstr(cmdCpy, "movestogo")))
            pos.time.movesToGo = atoi(argument + 10);

        if ((argument = strstr(cmdCpy, "movetime")))
            pos.time.moveTime = atoi(argument + 9);

        if ((argument = strstr(cmdCpy, "depth")))
            depth = atoi(argument + 6);

        if ((argument = strstr(cmdCpy, "nodes")))
            nodes = strtoull(argument + 6, NULL, 10);

        if ((argument = strstr(cmdCpy, "perft"))) {
            depth = atoi(argument + 6);
            perft = true;
        }
        
        if ((argument = strstr(cmdCpy, "ponder"))) {
            ponder = true;
            pos.time.ponder = true;
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
            }

            if (depth == -1) {
                depth = 64;
            }

            if (ponder) {
                depth = 64;
            } else {
                //Search::clearHashTable();ddd
            }
            Threads::StartSearch(pos, depth, numThreads, ponder, nodes);
        } else {
            if (pos.occupancies[Colors::both] == 0ULL)
                UCI::parsePosition(game, "position startpos");

            Perft::perftTest(depth, pos);
        }

        delete[] cmdCpy;
    }

    void UCI::bench() {
        static const char* benchLines[] = {
            "",
            "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 d2d3 f8c5",
            "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e7e5",
            "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1g5 e7e6",
            "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 d7d5 c4d5 f6d5",
            "e2e4 e7e6 d2d4 d7d5 b1c3 f8b4 e4e5 c7c5 a2a3 b4c3 b2c3",
            "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 e7e6 e2e3 b8d7",
            "d2d4 f7f5 c2c4 g8f6 g2g3 e7e6 f1g2 f8e7 g1f3 d7d5",
            "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6",
            "e2e4 d7d6 d2d4 g8f6 b1c3 g7g6 c1e3 f8g7 d1d2 c7c6",
            "g1f3 d7d5 g2g3 g8f6 f1g2 e7e6 c2c4 f8e7",
            "c2c4 g8f6 b1c3 e7e5 g1f3 b8c6 g2g3 f8b4",
            "e2e4 e7e5 g1f3 g8f6 f3e5 d7d6 e5f3 f6e4 d2d4 d6d5",
            "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 b8d7",
        };

        const int benchDepth = 13;
        U64 totalNodes = 0;

        Position clock;
        long long startMs = clock.time.getTimeMs();

        for (const char* line : benchLines) {
            Position pos;
            pos.parseFen(startPosition);

            char movesCopy[512];
            strcpy_s(movesCopy, sizeof(movesCopy), line);
            char* curChar = movesCopy;

            while (*curChar) {
                int move = parseMove(pos, curChar);
                if (move == 0) break;

                pos.repetitionIndex++;
                pos.repetitionTable[pos.repetitionIndex] = pos.hashKey;
                pos.makeMove(pos, move, MoveType::allMoves);

                while (*curChar && *curChar != ' ') curChar++;
                if (*curChar == ' ') curChar++;
            }

            Search::clearHashTable();
            pos.time.startTime = pos.time.getTimeMs();

            Threads::StartSearch(pos, benchDepth, 1, false, 0);
            Threads::WaitForSearchFinish();

            totalNodes += Threads::totalNodes.load(std::memory_order_relaxed);
        }

        long long elapsedMs = clock.time.getTimeMs() - startMs;
        if (elapsedMs <= 0) elapsedMs = 1;
        U64 nps = (totalNodes * 1000) / (U64)elapsedMs;

        printf("\n===========================\n");
        printf("Total time (ms) : %lld\n", elapsedMs);
        printf("Nodes searched  : %llu\n", (unsigned long long)totalNodes);
        printf("Nodes/second    : %llu\n", (unsigned long long)nps);
        fflush(stdout);
    }

    void UCI::loop() {
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
            } else if (strncmp(input, "position", 8) == 0) {
                UCI::parsePosition(game, input);
            } else if (strncmp(input, "ucinewgame", 10) == 0) {
                UCI::parsePosition(game, "position startpos");
                Search::clearHashTable(); // Clear TT only for new game
            } else if (strncmp(input, "go", 2) == 0) {
                UCI::parseGo(game, input);
            } else if (strncmp(input, "stop", 4) == 0) {
                Threads::stopFlag.store(true, std::memory_order_relaxed);
            } else if (strncmp(input, "ponderhit", 9) == 0) {
                // Elapsed pondering time keeps counting
                game.time.ponder = false;
                Threads::pondering.store(false, std::memory_order_relaxed);
            } else if (strncmp(input, "quit", 4) == 0) {
                Threads::stopFlag.store(true, std::memory_order_relaxed);
                Threads::WaitForSearchFinish();
                break;
            } else if (strncmp(input, "uci", 3) == 0) {
                printf("id name Sloth %s\n", VERSION);
                printf("id author William Sjolund\n");
                printf("option name Hash type spin default 64 min %d max %d\n", MIN_HASH, MAX_HASH);
                printf("option name Contempt type spin default 0 min 0 max 200\n");
                printf("option name Ponder type check default false\n");
                printf("option name Threads type spin default 1 min 1 max 256\n");
                Tune::printUCIOptions();
                printf("uciok\n");
            } else if (!strncmp(input, "setoption name Hash value ", 26)) {
                sscanf_s(input, "%*s %*s %*s %*s %d", &mbHash);
                if (mbHash < MIN_HASH) mbHash = MIN_HASH;
                if (mbHash > MAX_HASH) mbHash = MAX_HASH;
                Search::initHashTable(mbHash);
            } else if (!strncmp(input, "setoption name Threads value ", 29)) {
                sscanf_s(input, "%*s %*s %*s %*s %d", &numThreads);
                if (numThreads < 1) numThreads = 1;
                if (numThreads > 256) numThreads = 256;
                std::cout << "info string Using " << numThreads << " threads\n";
            } else if (!strncmp(input, "setoption name Contempt value ", 30)) {
                sscanf_s(input, "%*s %*s %*s %*s %d", &Search::contempt);
            } else if (!strncmp(input, "setoption name ", 15)) {
                const char* namePtr = input + 15;
                const char* valuePtr = strstr(namePtr, " value ");
                if (valuePtr) {
                    char paramName[128] = { 0 };
                    size_t nameLen = (size_t)(valuePtr - namePtr);
                    if (nameLen >= sizeof(paramName)) nameLen = sizeof(paramName) - 1;
                    memcpy(paramName, namePtr, nameLen);
                    paramName[nameLen] = '\0';

                    int value = atoi(valuePtr + 7); // skip over " value "
                    if (!Tune::setParam(paramName, value)) {
                        std::cout << "info string Unknown option " << paramName << "\n";
                    }
                }
            } else if (strncmp(input, "spsa", 4) == 0) {
                // "name, int, default, min, max, C_end, R_end"
                Tune::printSPSAInput();
            } else if (strncmp(input, "bench", 5) == 0) {
                UCI::bench();
            }
        }
    }
}

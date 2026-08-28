#ifndef THREADS_H
#define THREADS_H

#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include "position.h"
#include "types.h"

namespace Sloth {
    namespace Threads {
        constexpr int CONT_HIST_SIZE = 12 * 64 * 12 * 64;
        inline int contHistIndex(int prevPiece, int prevTarget, int piece, int target) {
            return ((prevPiece * 64 + prevTarget) * 12 + piece) * 64 + target;
        }

        struct ThreadData {
            int threadId;
            std::atomic<bool>* stop;
            Position pos;  // Each thread gets its own copy
            int score;
            int ply;
            int maxPly;
            int depth;
            U64 nodes;
            double agingFactor;
            
            bool followPV;
            bool scorePV;

            SearchStack ss[MAX_PLY];

            int pvLength[MAX_PLY];
            int pvTable[MAX_PLY][MAX_PLY];
            int killerMoves[2][MAX_PLY];
            int historyMoves[12][64];
            std::vector<int> continuationHistory;
        };

        extern std::vector<std::thread> threads;
        extern std::vector<ThreadData> threadPool;
        extern std::atomic<bool> stopFlag;
        extern std::atomic<U64> totalNodes;
        extern U64 nodeLimit;

        extern std::atomic<bool> pondering;
        extern std::atomic<bool> timeControlActive;
        extern std::atomic<int> startTimeMs;
        extern std::atomic<int> stopTimeMs;
        extern std::atomic<bool> searchRunning;

        void InitializeThreads(int numThreads, Position& pos);
        void StartSearch(Position& pos, int depth, int numThreads, bool ponder, U64 nodes = 0);
        void WaitForSearchFinish();
        void RunSearch(ThreadData* threadData);
        bool tryVisitNode(ThreadData* threadData);
    }
}

#endif

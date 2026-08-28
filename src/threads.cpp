#include "threads.h"
#include "search.h"
#include "movegen.h"
#include <atomic>

namespace Sloth {
    namespace Threads {
        std::vector<std::thread> threads;
        std::vector<ThreadData> threadPool;
        std::atomic<bool> stopFlag{false};
        std::atomic<U64> totalNodes{0};
        U64 nodeLimit = 0;

        std::atomic<bool> pondering{false};
        std::atomic<bool> timeControlActive{false};
        std::atomic<int> startTimeMs{0};
        std::atomic<int> stopTimeMs{0};
        std::atomic<bool> searchRunning{false};

        std::thread searchDriverThread;

        bool tryVisitNode(ThreadData* threadData) {
            if (stopFlag.load(std::memory_order_relaxed)) {
                return false;
            }

            if (nodeLimit != 0) {
                U64 current = totalNodes.load(std::memory_order_relaxed);
                while (current < nodeLimit) {
                    if (totalNodes.compare_exchange_weak(current, current + 1,
                                                         std::memory_order_relaxed,
                                                         std::memory_order_relaxed)) {
                        ++threadData->nodes;
                        return true;
                    }
                }

                stopFlag.store(true, std::memory_order_relaxed);
                return false;
            }

            totalNodes.fetch_add(1, std::memory_order_relaxed);
            ++threadData->nodes;
            return true;
        }

        void InitializeThreads(int numThreads, Position& pos) {
            threadPool.clear();
            threads.clear();
            stopFlag.store(false, std::memory_order_relaxed);

            for (int i = 0; i < numThreads; i++) {
                ThreadData td;
                td.threadId = i;
                td.stop = &stopFlag;
                
                // Each thread gets its own independent copy of the position
                td.pos = pos;  
                
                td.score = 0;
                td.ply = 0;
                td.maxPly = 0;
                td.depth = 0;
                td.nodes = 0;
                td.agingFactor = 0.9;
                td.followPV = false;
                td.scorePV = false;
                
                // Initialize arrays to zero
                memset(td.ss, 0, sizeof(td.ss));
                memset(td.pvLength, 0, sizeof(td.pvLength));
                memset(td.pvTable, 0, sizeof(td.pvTable));
                memset(td.killerMoves, 0, sizeof(td.killerMoves));
                memset(td.historyMoves, 0, sizeof(td.historyMoves));
                td.continuationHistory.assign(CONT_HIST_SIZE, 0);

                threadPool.push_back(std::move(td));
            }
        }

        void RunSearch(ThreadData* threadData) {
            try {
                Search::iterativeDeepen(threadData);
            } catch (const std::exception& e) {
                std::cerr << "Thread " << threadData->threadId << " error: " << e.what() << std::endl;
            }
        }

        static void searchDriver() {
            int numThreads = (int)threadPool.size();

            // Start helper threads (threadId 1 to numThreads-1)
            threads.reserve(numThreads > 0 ? numThreads - 1 : 0);
            for (int i = 1; i < numThreads; i++) {
                threads.emplace_back(RunSearch, &threadPool[i]);
            }

            // Main search thread (threadId 0) also searches
            RunSearch(&threadPool[0]);

            // Signal all threads to stop
            stopFlag.store(true, std::memory_order_relaxed);

            // Join helper threads
            for (auto& t : threads) {
                if (t.joinable()) {
                    t.join();
                }
            }

            threads.clear();

            printf("bestmove ");
            Movegen::printMove(threadPool[0].pvTable[0][0]);
            if (threadPool[0].pvLength[0] > 1) {
                printf(" ponder ");
                Movegen::printMove(threadPool[0].pvTable[0][1]);
            }
            printf("\n");
            fflush(stdout);

            searchRunning.store(false, std::memory_order_relaxed);
        }

        void WaitForSearchFinish() {
            if (searchDriverThread.joinable()) {
                searchDriverThread.join();
            }
        }

        void StartSearch(Position& pos, int depth, int numThreads, bool ponder, U64 nodes) {
            WaitForSearchFinish();

            InitializeThreads(numThreads, pos);
            nodeLimit = nodes;
            totalNodes.store(0, std::memory_order_relaxed);

            for (auto& td : threadPool) {
                td.depth = depth;
                td.pos.time.ponder = ponder;
            }

            pondering.store(ponder, std::memory_order_relaxed);
            timeControlActive.store(pos.time.timeSet == 1, std::memory_order_relaxed);
            startTimeMs.store(pos.time.startTime, std::memory_order_relaxed);
            stopTimeMs.store(pos.time.stopTime, std::memory_order_relaxed);

            searchRunning.store(true, std::memory_order_relaxed);
            searchDriverThread = std::thread(searchDriver);
        }
    }
}

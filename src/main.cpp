/*
    Big thanks to:
    https://www.youtube.com/@chessprogramming591
    https://www.chessprogramming.org/

*/

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <string.h>

#include <chrono>

#include "uci.h"
#include "piece.h"
#include "magic.h"
#include "movegen.h"
#include "types.h"

#include "position.h"
#include "perft.h"
#include "uci.h"
#include "search.h"
#include "evaluate.h"
#include "nnue.h"
#include "tune.h"

using namespace Sloth;
int main(int argc, char* argv[])
{
    if (argc > 1 && strcmp(argv[1], "spsa") == 0) {
        Tune::printSPSAInput();
        return 0;
    }

    Magic::initAttacks();
    Bitboards::initLeaperAttacks();
    Zobrist::initRandomKeys();
    Search::initHashTable(64);

    const bool debug = false;

    if (nn_load("eval.nnue") != 0) {
        std::cout << "info string Failed to load NNUE file: eval.nnue\n";
    }

    if (argc > 1 && strcmp(argv[1], "bench") == 0) {
        UCI::bench();
        goto end;
    }

    if (debug) {
        Position pos;
        pos.debug = true;

        Movegen::MoveList movelist[1];

        const char* testFens[] = {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        };

        for (const char* fen : testFens) {
            pos.parseFen(fen);
            pos.printBoard();

            int score = Eval::evaluate(pos);

            std::cout << "eval: " << score << std::endl;
        }

        #ifdef _WIN32
            system("pause");
        #else
           system("read -p 'Press Enter to continue...' var");
        #endif
    } else UCI::loop();
end:
    free(Search::hashTable);

    return 0;
}

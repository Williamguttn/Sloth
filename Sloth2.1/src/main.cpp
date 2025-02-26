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

// TODO: set cxx path in build_windows to smth else (nothing)
// TODO: add uci ponder option
// TODO: find changes made since last version. I know:
// 1. simple history aging is added
// 2. Move pondering
// 3. Cross compatibility added
// 4. Search optimizations

using namespace Sloth;

int main(int argc, char* argv[])
{
    Magic::initAttacks();
    Bitboards::initLeaperAttacks();
    Zobrist::initRandomKeys();
    Search::initHashTable(64);
    Eval::initEvalMasks();

    bool debug = false;

    if (debug) {
        Position pos;

        Movegen::MoveList movelist[1];

        pos.parseFen("8/8/8/8/8/8/6K1/8 w - - 0 1");       
        pos.printBoard();
        Eval::evaluate(pos);
    } else UCI::loop();

    my_free(Search::hashTable);

    return 0;
}

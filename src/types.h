#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

//#include <cstdint>
#include <atomic> //#temp
#include <cstdint>

#include "bitboards.h"
#pragma warning(disable: 4554)

#define VERSION "2.2"

#define emptyBoard "8/8/8/8/8/8/8/8 b - - "
#define startPosition "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define trickyPosition "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
#define killerPosition "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define repetitions "2r3k1/R7/8/1R6/8/8/P4KPP/8 w - - 0 1"

#define VALUE_INFINITE 50000

#define MAX_PLY 64

#define NO_HASH_ENTRY 100000
//#define MAX_HASH 256 // max hash 128 mb
#define MIN_HASH 16
#define MAX_HASH 1000000

#define hashfEXACT 0
#define hashfALPHA 1
#define hashfBETA 2

#define EVAL_UNKNOWN 32000
#define MATE_VALUE 49000
#define MATE_SCORE 48000

#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define CACHE_LINE_SIZE 64

// Lockless hashing, keyXorData = hashKey ^ data, torn concurrent read fails key check instead of returning garbage
typedef struct HASHE {
    std::atomic<uint64_t> keyXorData;
    std::atomic<uint64_t> data;
    char padding[
        CACHE_LINE_SIZE -
        2 * sizeof(std::atomic<uint64_t>)
    ];
} HashEntry;

struct SearchStack {
    int ply;
    int staticEval;
    int currentMove;
    bool tactical;
};

enum {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, no_sq
};

enum Colors : int {
    white, black, both
};

/*
    0001    1 White king can castle to king side
    0010    2 White king can castle to queen side
    0100    4 Black king to king side
    1000    8 Black king to queen side
*/
enum CastlingRights : uint8_t {
    WK = 1, WQ = 2, BK = 4, BQ = 8
};

const int CASTLING_RIGHTS_CONSTANTS[64] = {
    7, 15, 15, 15,  3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14
};

enum MoveType {
    allMoves, captures
};

// will be used to mirror squares for opposite side (Example: e4 becomes e5 for black)
const int MIRROR_SCORE[128] =
{
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8
};

// most valuable victim - least valuable agressor
static int MVV_LVA[12][12] = { // attacker, victim
    105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
    104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
    103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
    102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
    101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
    100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600,

    105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
    104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
    103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
    102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
    101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
    100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600
};

#endif
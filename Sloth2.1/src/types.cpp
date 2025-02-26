#include "types.h"

int materialScore[12] = {
    100, // white pawn
    300, // white knight (pawn * 3)
    350, // white bishop (pawn * 3.5)
    500, // white rook (pawn * 5)
    1000, // white queen (pawn * 10)
    10000, // white king (pawn * 100)
    -100,
    -300,
    -350,
    -500,
    -1000,
    -10000
};
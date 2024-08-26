#ifndef BITBOARDS_H
#define BITBOARDS_H

#include <cstdint>
#include <cassert>
#include <vector>

#define setBit(bb, sq) (bb |= (1ULL << sq))
#define getBit(bb, sq) (bb & (1ULL << sq))
#define removeBit(bb, sq) (bb &= ~(1ULL << sq))
#define popBit(bb, sq) (getBit(bb, sq) ? bb ^= (1ULL << sq) : 0) // invert bit

typedef uint64_t U64;

namespace Sloth {

	namespace Bitboards {

		extern const U64 notAFile;
		extern const U64 notHFile;

		extern const U64 notHGFile;
		extern const U64 notABFile;

		extern U64 pawnAttacks[2][64]; // [side][square]
		extern U64 knightAttacks[64]; // [square]
		extern U64 kingAttacks[64];

		// 12 bitboards, each piece and each color
		extern U64 bitboards[12];
		extern U64 occupancies[3]; // This will hold every piece on one bitboard. One for every white piece combined, one for black combined and one with every piece of both color on the bitboard

		void printBitboard(U64 bb, bool flip);

		U64 openFileCount(U64 pawns);
		U64 pawnAdvance(U64 pawns, U64 occ, int color);

		U64 maskPawnAttacks(int square, int side);
		U64 maskKnightAttacks(int square);
		U64 maskKingAttacks(int square);

		void initLeaperAttacks();

		extern  int countBits(U64 bitboard); // counts every bit available on a bitboard

		// get least significant 1st bit index
		extern int getLs1bIndex(U64 bitboard);
		extern int msb(U64 bb);
	}
}

#endif
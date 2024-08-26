#include <iostream>
#include <string.h>

#include "magic.h"
#include "bitboards.h"
#include "piece.h"

namespace Sloth {
    U64 Magic::bishopMasks[64];
    U64 Magic::rookMasks[64];

    U64 Magic::bishopAttacks[64][512];
    U64 Magic::rookAttacks[64][4096];

    //U64 Magic::bishopMagics[64];

	U64 Magic::maskBishopAttacks(int square) {
		U64 attacks = 0ULL;

		int rank, file;
		int targetRank = square / 8;
		int targetFile = square % 8;

		// Rank & file to square: rank * 8 + file
		for (rank = targetRank + 1, file = targetFile + 1; rank <= 6 && file <= 6; rank++, file++) attacks |= (1ULL << (rank * 8 + file));
		for (rank = targetRank - 1, file = targetFile + 1; rank >= 1 && file <= 6; rank--, file++) attacks |= (1ULL << (rank * 8 + file));
		for (rank = targetRank + 1, file = targetFile - 1; rank <= 6 && file >= 1; rank++, file--) attacks |= (1ULL << (rank * 8 + file));
		for (rank = targetRank - 1, file = targetFile - 1; rank >= 1 && file >= 1; rank--, file--) attacks |= (1ULL << (rank * 8 + file));

		return attacks;
	}

	U64 Magic::maskRookAttacks(int square) {
		U64 attacks = 0ULL;

		int r, f;
		int tr = square / 8;
		int tf = square % 8;

		for (r = tr + 1; r <= 6; r++) attacks |= (1ULL << (r * 8 + tf));
		for (r = tr - 1; r >= 1; r--) attacks |= (1ULL << (r * 8 + tf));
		for (f = tf + 1; f <= 6; f++) attacks |= (1ULL << (tr * 8 + f));
		for (f = tf - 1; f >= 1; f--) attacks |= (1ULL << (tr * 8 + f));

		return attacks;
	}

    // generate bishop attacks on the fly
    U64 bishopAttacksOnTheFly(int square, U64 block)
    {
        U64 attacks = 0ULL;

        int r, f;

        int tr = square / 8;
        int tf = square % 8;

        for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++)
        {
            attacks |= (1ULL << (r * 8 + f));
            if ((1ULL << (r * 8 + f)) & block) break;
        }

        for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++)
        {
            attacks |= (1ULL << (r * 8 + f));
            if ((1ULL << (r * 8 + f)) & block) break;
        }

        for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--)
        {
            attacks |= (1ULL << (r * 8 + f));
            if ((1ULL << (r * 8 + f)) & block) break;
        }

        for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--)
        {
            attacks |= (1ULL << (r * 8 + f));
            if ((1ULL << (r * 8 + f)) & block) break;
        }

        return attacks;
    }

    // generate rook attacks on the fly
    U64 rookAttacksOnTheFly(int square, U64 block)
    {
        U64 attacks = 0ULL;

        int r, f;

        int tr = square / 8;
        int tf = square % 8;

        for (r = tr + 1; r <= 7; r++)
        {
            attacks |= (1ULL << (r * 8 + tf));
            if ((1ULL << (r * 8 + tf)) & block) break;
        }

        for (r = tr - 1; r >= 0; r--)
        {
            attacks |= (1ULL << (r * 8 + tf));
            if ((1ULL << (r * 8 + tf)) & block) break;
        }

        for (f = tf + 1; f <= 7; f++)
        {
            attacks |= (1ULL << (tr * 8 + f));
            if ((1ULL << (tr * 8 + f)) & block) break;
        }

        for (f = tf - 1; f >= 0; f--)
        {
            attacks |= (1ULL << (tr * 8 + f));
            if ((1ULL << (tr * 8 + f)) & block) break;
        }

        return attacks;
    }


    unsigned int Magic::getRandomU32Num() {
        unsigned int num = randomState;

        num ^= num << 13;
        num ^= num >> 17;
        num ^= num << 5;

        randomState = num;

        return num;
    }

    U64 Magic::getRandomU64Num() {
        U64 n1, n2, n3, n4;

        n1 = (U64)(getRandomU32Num() & 0xFFFF);
        n2 = (U64)(getRandomU32Num() & 0xFFFF);
        n3 = (U64)(getRandomU32Num() & 0xFFFF);
        n4 = (U64)(getRandomU32Num() & 0xFFFF);

        return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48); // shuffle bits
    }

    U64 Magic::generateMagicNumber() {
        return getRandomU64Num() & getRandomU64Num() & getRandomU64Num();
    }

	U64 Magic::setOccupancy(int index, int bitsInMask, U64 attackMask) {
		U64 occupancy = 0ULL;

		// loop over range of bits within attack mask
		for (int c = 0; c < bitsInMask; c++) {
			int square = Bitboards::getLs1bIndex(attackMask);

			// pop LS1B in the attack map
			popBit(attackMask, square);

			if (index & (1 << c)) {
				occupancy |= (1ULL << square);
			}
		}

		return occupancy;
	}

	U64 Magic::findMagicNumber(int square, int relevantBits, int bishop) {
		U64 occupancies[4096]; // Rook occupancies

		// attack tables
		U64 attacks[4096];

		U64 usedAttacks[4096];

		U64 attackMask = bishop ? maskBishopAttacks(square) : maskRookAttacks(square);

		int occupancyIndicies = 1 << relevantBits;

		for (int i = 0; i < occupancyIndicies; i++) {
			occupancies[i] = setOccupancy(i, relevantBits, attackMask);

            attacks[i] = bishop ? bishopAttacksOnTheFly(square, occupancies[i]) : rookAttacksOnTheFly(square, occupancies[i]);
		}

        // test magic numbers loop
        for (int r = 0; r < 100000000; r++) {
            // generate magic number candidate
            U64 magicNumber = generateMagicNumber();

            // skip innapropriate magic numbers
            if (Bitboards::countBits((attackMask * magicNumber) & 0xFF00000000000000) < 6) continue;

            memset(usedAttacks, 0ULL, sizeof(usedAttacks));

            int i, fail;

            // test magic index loop

            for(i = 0, fail = 0; !fail && i < occupancyIndicies; i++) {
                int magicIndex = (int)((occupancies[i] * magicNumber) >> (64 - relevantBits));

                if (usedAttacks[magicIndex] == 0ULL) {
                    usedAttacks[magicIndex] = attacks[i];
                }
                else if (usedAttacks[magicIndex] != attacks[i]) {
                    // Magic index doesnt work
                    fail = 1;
                }
            }

            // magic number works
            if (!fail) {
                return magicNumber;
            }
        }

        std::cout << "Magic number fails." << std::endl;

        return 0ULL;
	}

    void Magic::initSlidersAttacks(int bishop) {
        for (int square = 0; square < 64; square++) {
            // init bishop & rook masks
            bishopMasks[square] = maskBishopAttacks(square);
            rookMasks[square] = maskRookAttacks(square);

            U64 attackMask = bishop ? bishopMasks[square] : rookMasks[square];

            int relevantBits = Bitboards::countBits(attackMask);
            int occupancyIndicies = (1 << relevantBits);

            for (int i = 0; i < occupancyIndicies; i++) {
                if (bishop) {
                    U64 occupancy = setOccupancy(i, relevantBits, attackMask);

                    // init magic index
                    int magicIndex = (occupancy * bishopMagics[square]) >> (64 - BISHOP_RELEVANT_BITS[square]);

                    bishopAttacks[square][magicIndex] = bishopAttacksOnTheFly(square, occupancy);
                }
                else {
                    U64 occupancy = setOccupancy(i, relevantBits, attackMask);
                    int magicIndex = (occupancy * rookMagics[square]) >> (64 - ROOK_RELEVANT_BITS[square]);

                    rookAttacks[square][magicIndex] = rookAttacksOnTheFly(square, occupancy);
                }
            }
        }
    }
    
    // everything self explanatory
     U64 Magic::getBishopAttacks(int square, U64 occupancy) {
        occupancy &= bishopMasks[square];
        occupancy *= bishopMagics[square];
        occupancy >>= 64 - BISHOP_RELEVANT_BITS[square];

        return bishopAttacks[square][occupancy];
    }

     U64 Magic::getRookAttacks(int square, U64 occupancy) {
        occupancy &= rookMasks[square];
        occupancy *= rookMagics[square];
        occupancy >>= 64 - ROOK_RELEVANT_BITS[square];

        return rookAttacks[square][occupancy];
    }

     U64 Magic::getQueenAttacks(int square, U64 occupancy) {
        U64 bishopAttacks = getBishopAttacks(square, occupancy);
        U64 rookAttacks = getRookAttacks(square, occupancy);

        return (bishopAttacks | rookAttacks);
    }

    void Magic::initAttacks() {
        initSlidersAttacks(Piece::bishop);
        initSlidersAttacks(Piece::rook);
    }

    void Magic::initMagicNumbers() {
        /*for (int square = 0; square < 64; square++) {
            printf(" 0x%lluxULL\n", findMagicNumber(square, ROOK_RELEVANT_BITS[square], Piece::rook));

            //findMagicNumber(square, BISHOP_RELEVANT_BITS[square], 1);
        }*/

        for (int square = 0; square < 64; square++) {
            rookMagics[square] = findMagicNumber(square, ROOK_RELEVANT_BITS[square], Piece::rook);
            //printf(" 0x%llxULL,\n", findMagicNumber(square, ROOK_RELEVANT_BITS[square], Piece::rook));
        }

        //std::cout << std::endl;

        for (int square = 0; square < 64; square++) {
            bishopMagics[square] = findMagicNumber(square, BISHOP_RELEVANT_BITS[square], Piece::bishop);

            //printf(" 0x%llxULL,\n", findMagicNumber(square, BISHOP_RELEVANT_BITS[square], Piece::bishop));
        }
    }
}
#ifndef MAGIC_H_INCLUDED
#define MAGIC_H_INCLUDED

#include "bitboards.h"

namespace Sloth {
    namespace Magic {
        extern U64 bishopMasks[64];
        extern U64 rookMasks[64];

        extern U64 bishopAttacks[64][512]; // [square][occupancies]
        extern U64 rookAttacks[64][4096];

        const int BISHOP_RELEVANT_BITS[64] = {
            6, 5, 5, 5, 5, 5, 5, 6,
            5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 7, 7, 7, 7, 5, 5,
            5, 5, 7, 9, 9, 7, 5, 5,
            5, 5, 7, 9, 9, 7, 5, 5,
            5, 5, 7, 7, 7, 7, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5,
            6, 5, 5, 5, 5, 5, 5, 6,
        };

        const int ROOK_RELEVANT_BITS[64] = {
            12, 11, 11, 11, 11, 11, 11, 12,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            12, 11, 11, 11, 11, 11, 11, 12,
        };


        // DEFINE MAGIC NUMBERS

        static U64 rookMagics[64] = {
            0xa080041440042080ULL,
            0xc0200040001000ULL,
            0x180200081100008ULL,
            0x2080100004820800ULL,
            0x8200041002000920ULL,
            0x100040018066900ULL,
            0x400440088011002ULL,
            0x100004081000022ULL,
            0x100802080004008ULL,
            0x800402010004000ULL,
            0x4002001200402084ULL,
            0x5080800800801000ULL,
            0x8414800800040280ULL,
            0x12001008420004ULL,
            0x42003200840148ULL,
            0x200032480440aULL,
            0x228002400080ULL,
            0x30004000200050ULL,
            0x4002020010884020ULL,
            0x888008010000880ULL,
            0x132828004000800ULL,
            0x840818012000400ULL,
            0x4500240008619002ULL,
            0x400020013004684ULL,
            0x80004040002000ULL,
            0x8800200480400380ULL,
            0x8080200100410010ULL,
            0x90100180080081ULL,
            0x80100100500ULL,
            0x44000480020080ULL,
            0x201211400021008ULL,
            0x1288210600044084ULL,
            0x300401020800080ULL,
            0x4142010042003080ULL,
            0x20811002802001ULL,
            0x518020010100100ULL,
            0x100801000500ULL,
            0x56002004040010ULL,
            0x810025004004108ULL,
            0xc06004082000401ULL,
            0x4180804000208002ULL,
            0x89200040008a8020ULL,
            0x8200011010040ULL,
            0x100080010008080ULL,
            0x50008010011ULL,
            0x1006001810060004ULL,
            0x480218015004008aULL,
            0x5101284400820009ULL,
            0x800211480084100ULL,
            0x112400688210100ULL,
            0x401108200100ULL,
            0xc000100080080480ULL,
            0x4400080004008080ULL,
            0x202000204008080ULL,
            0x402080210213400ULL,
            0x80c0120984200ULL,
            0x6000820410214302ULL,
            0x45011420400281ULL,
            0x404290020029141ULL,
            0x800140821700101ULL,
            0x103000210080005ULL,
            0x448100240008820dULL,
            0x51000200d1100804ULL,
            0x410008024010042ULL,
        };

        static U64 bishopMagics[64] = { // UPDATED BISHOP MAGICS
            0x40040844404084ULL,
            0x2004208a004208ULL,
            0x10190041080202ULL,
            0x108060845042010ULL,
            0x581104180800210ULL,
            0x2112080446200010ULL,
            0x1080820820060210ULL,
            0x3c0808410220200ULL,
            0x4050404440404ULL,
            0x21001420088ULL,
            0x24d0080801082102ULL,
            0x1020a0a020400ULL,
            0x40308200402ULL,
            0x4011002100800ULL,
            0x401484104104005ULL,
            0x801010402020200ULL,
            0x400210c3880100ULL,
            0x404022024108200ULL,
            0x810018200204102ULL,
            0x4002801a02003ULL,
            0x85040820080400ULL,
            0x810102c808880400ULL,
            0xe900410884800ULL,
            0x8002020480840102ULL,
            0x220200865090201ULL,
            0x2010100a02021202ULL,
            0x152048408022401ULL,
            0x20080002081110ULL,
            0x4001001021004000ULL,
            0x800040400a011002ULL,
            0xe4004081011002ULL,
            0x1c004001012080ULL,
            0x8004200962a00220ULL,
            0x8422100208500202ULL,
            0x2000402200300c08ULL,
            0x8646020080080080ULL,
            0x80020a0200100808ULL,
            0x2010004880111000ULL,
            0x623000a080011400ULL,
            0x42008c0340209202ULL,
            0x209188240001000ULL,
            0x400408a884001800ULL,
            0x110400a6080400ULL,
            0x1840060a44020800ULL,
            0x90080104000041ULL,
            0x201011000808101ULL,
            0x1a2208080504f080ULL,
            0x8012020600211212ULL,
            0x500861011240000ULL,
            0x180806108200800ULL,
            0x4000020e01040044ULL,
            0x300000261044000aULL,
            0x802241102020002ULL,
            0x20906061210001ULL,
            0x5a84841004010310ULL,
            0x4010801011c04ULL,
            0xa010109502200ULL,
            0x4a02012000ULL,
            0x500201010098b028ULL,
            0x8040002811040900ULL,
            0x28000010020204ULL,
            0x6000020202d0240ULL,
            0x8918844842082200ULL,
            0x4010011029020020ULL
        };

        static unsigned int randomState = 1804289383; // psuedo random number state

        U64 maskBishopAttacks(int square);
        U64 maskRookAttacks(int square);
        U64 setOccupancy(int index, int bitsInMask, U64 attackMask);

        unsigned int getRandomU32Num();

        U64 getRandomU64Num();
        U64 generateMagicNumber();

        void initSlidersAttacks(int bishop);

        extern  U64 getBishopAttacks(int square, U64 occupancy);
        extern  U64 getRookAttacks(int square, U64 occupancy);
        extern  U64 getQueenAttacks(int square, U64 occupancy);

        void initAttacks();

        U64 findMagicNumber(int square, int relevantBits, int bishop);
        void initMagicNumbers();
    }
}
#endif
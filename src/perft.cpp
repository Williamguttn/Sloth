#include <cstdint>
#include <string>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include "perft.h"
#include "position.h"
#include "movegen.h"

namespace Sloth {

    uint64_t Perft::nodes = 0;

    void Perft::perft(int depth, Position& pos) {
        if (depth == 0) {
            ++nodes;
            return;
        }

        Movegen::MoveList moveList;
        Movegen::generateMoves(pos, &moveList, /*onlyCaptures=*/false);

        for (int c = 0; c < moveList.count; ++c) {
            copyBoard(pos);
            if (pos.makeMove(pos, moveList.moves[c], MoveType::allMoves)) {
                perft(depth - 1, pos);
                takeBack(pos);
            }
        }
    }

    std::string formatNumber(uint64_t n) {
        std::ostringstream oss;
        if (n >= 1'000'000'000ULL) {
            oss << std::fixed << std::setprecision(2) << (n / 1e9) << "B";
        } else if (n >= 1'000'000ULL) {
            oss << std::fixed << std::setprecision(2) << (n / 1e6) << "M";
        } else if (n >= 1'000ULL) {
            oss << std::fixed << std::setprecision(2) << (n / 1e3) << "K";
        } else {
            oss << n;
        }
        return oss.str();
    }

    void Perft::perftTest(int depth, Position& pos) {
        static const char* squareToCoordinates[] = {
            "a8","b8","c8","d8","e8","f8","g8","h8",
            "a7","b7","c7","d7","e7","f7","g7","h7",
            "a6","b6","c6","d6","e6","f6","g6","h6",
            "a5","b5","c5","d5","e5","f5","g5","h5",
            "a4","b4","c4","d4","e4","f4","g4","h4",
            "a3","b3","c3","d3","e3","f3","g3","h3",
            "a2","b2","c2","d2","e2","f2","g2","h2",
            "a1","b1","c1","d1","e1","f1","g1","h1"
        };

        nodes = 0;
        printf("\nPerft\n");

        Movegen::MoveList moveList;
        Movegen::generateMoves(pos, &moveList, /*onlyCaptures=*/false);

        long startMs = getTimeMs();

        for (int c = 0; c < moveList.count; ++c) {
            copyBoard(pos);
            auto m = moveList.moves[c];
            if (pos.makeMove(pos, m, MoveType::allMoves)) {
                uint64_t before = nodes;
                perft(depth - 1, pos);
                uint64_t thisCount = nodes - before;

                int from = getMoveSource(m);
                int to   = getMoveTarget(m);
                char promo = Movegen::promotedPieces[
                    static_cast<Piece::Pieces>(getMovePromotion(m))
                ];

                printf("Move: %s%s%c  Nodes: %llu\n",
                    squareToCoordinates[from],
                    squareToCoordinates[to],
                    promo ? promo : ' ',
                    (unsigned long long)thisCount
                );

                takeBack(pos);
            }
        }

        long elapsed = getTimeMs() - startMs;
        uint64_t nps = elapsed
            ? (nodes * 1000ULL / static_cast<uint64_t>(elapsed))
            : nodes;

        printf(
            "\nDepth: %d\n"
            "Total nodes: %llu\n"
            "Nodes/sec: %s\n"
            "Time: %ld ms\n",
            depth,
            (unsigned long long)nodes,
            formatNumber(nps).c_str(),
            elapsed
        );
    }

}
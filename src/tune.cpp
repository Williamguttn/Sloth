#include <cstdio>
#include <cstring>
#include <algorithm>

#include "tune.h"

namespace Sloth {
namespace Tune {

    std::vector<TunableParam>& params() {
        static std::vector<TunableParam> instance;
        return instance;
    }

    Registrar::Registrar(const char* name, int* value, int defaultValue, int min, int max, int step) {
        params().push_back({ name, value, defaultValue, min, max, step });
    }

    void printUCIOptions() {
        for (const auto& p : params()) {
            printf("option name %s type spin default %d min %d max %d\n", p.name, p.defaultValue, p.min, p.max);
        }
    }

    bool setParam(const char* name, int value) {
        for (auto& p : params()) {
            if (strcmp(p.name, name) == 0) {
                *p.value = std::clamp(value, p.min, p.max);
                return true;
            }
        }
        return false;
    }

    // OpenBench SPSA input box format: 
    // "name, int, default, min, max, C_end, R_end"
    void printSPSAInput() {
        for (const auto& p : params()) {
            printf("%s, int, %d, %d, %d, %.2f, 0.002\n", p.name, p.defaultValue, p.min, p.max, (double)p.step);
        }
    }

} // namespace Tune

// Reverse futility / static-eval pruning
TUNE_PARAM(RfpQMargin,                 273,      100,   500,   20);
TUNE_PARAM(RfpMargin1PerDepth,         127,      50,    200,   8);
TUNE_PARAM(RfpMargin2PerDepth,         80,       20,    150,   6);

// Futility pruning (main move loop)
TUNE_PARAM(FutilityMarginPerDepth,     139,      60,    250,   10);
TUNE_PARAM(FutilityMaxDepth,           9,        4,     12,    1);

// Razoring
TUNE_PARAM(RazorBaseMargin,            255,      100,   500,   20);
TUNE_PARAM(RazorMarginPerDepth,        310,      100,   500,   20);
TUNE_PARAM(RazorMaxDepth,              3,        2,     6,     1);

// ProbCut
TUNE_PARAM(ProbCutMargin,              170,      80,    300,   15);
TUNE_PARAM(ProbCutMinDepth,            4,        2,     10,    1);
TUNE_PARAM(ProbCutReduction,           4,        2,     6,     1);
TUNE_PARAM(ProbCutTTDepthMargin,       6,        1,     10,    1);

// Null move pruning
TUNE_PARAM(NmpMinDepth,               2,        1,     6,     1);
TUNE_PARAM(NmpBaseReduction,           4,        1,     7,     1);
TUNE_PARAM(NmpDepthBonusReduction,     2,        0,     5,     1);
TUNE_PARAM(NmpDepthThreshold,          8,        6,     12,    1);

// Late move pruning: margin(depth) = LmpBase + LmpMult * depth * depth
TUNE_PARAM(LmpMaxDepth,                5,        2,     8,     1);
TUNE_PARAM(LmpBase,                    6,        0,     16,    2);
TUNE_PARAM(LmpMult,                    5,        1,     6,     1);

// History pruning
TUNE_PARAM(HistoryPruningMargin,       8000,     1000,  8000,  250);
TUNE_PARAM(HistoryPruningMaxDepth,     4,        2,     8,     1);

// Late move reduction
TUNE_PARAM(LmrMinDepth,                4,        2,     6,     1);
TUNE_PARAM(LmrMinMoveCount,            4,        1,     8,     1);
TUNE_PARAM(LmrBase100,                 67,       0,     150,   10);
TUNE_PARAM(LmrDivisor100,              154,      100,   400,   15);
TUNE_PARAM(LmrPvReduction,             0,        0,     2,     1);
TUNE_PARAM(LmrHistoryDivisor,          8192,     2048,  16384, 1000);

// Aspiration window
TUNE_PARAM(AspirationWindow,           52,       10,    100,   5);

// History heuristic
TUNE_PARAM(HistoryMalusDivisor,        1,        1,     4,     1);
TUNE_PARAM(HistBonusMul,               300,      100,   600,   30);
TUNE_PARAM(HistBonusSub,               250,      0,     600,   40);
TUNE_PARAM(HistBonusMax,               2500,     1000,  4000,  200);
TUNE_PARAM(HistoryPlyDivisor,          4,        1,     8,     1);
TUNE_PARAM(CaptureAttackerDivisor,     9,        2,     16,    1);
TUNE_PARAM(CaptureSeeDivisor,          4,        1,     8,     1);

} // namespace Sloth

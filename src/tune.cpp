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
TUNE_PARAM(RfpQMargin,                 339,      100,   500,   20);
TUNE_PARAM(RfpMargin1PerDepth,         120,      50,    200,   8);
TUNE_PARAM(RfpMargin2PerDepth,         65,       20,    150,   6);

// Futility pruning (main move loop)
TUNE_PARAM(FutilityMarginPerDepth,     168,      60,    250,   10);
TUNE_PARAM(FutilityMaxDepth,           8,        4,     12,    1);

// Razoring
TUNE_PARAM(RazorBaseMargin,            300,      100,   500,   20);
TUNE_PARAM(RazorMarginPerDepth,        300,      100,   500,   20);
TUNE_PARAM(RazorMaxDepth,              3,        2,     6,     1);

// Internal iterative reduction
TUNE_PARAM(IirMinDepth,                6,        4,     10,    1);

// ProbCut
TUNE_PARAM(ProbCutMargin,              172,      80,    300,   15);
TUNE_PARAM(ProbCutMinDepth,            6,        4,     10,    1);
TUNE_PARAM(ProbCutReduction,           4,        2,     6,     1);
TUNE_PARAM(ProbCutTTDepthMargin,       3,        1,     6,     1);

// Null move pruning
TUNE_PARAM(NmpMinDepth,                3,        2,     6,     1);
TUNE_PARAM(NmpBaseReduction,           2,        1,     4,     1);
TUNE_PARAM(NmpDepthBonusReduction,     1,        0,     3,     1);
TUNE_PARAM(NmpDepthThreshold,          8,        6,     12,    1);

// Late move pruning: margin(depth) = LmpBase + LmpMult * depth * depth
TUNE_PARAM(LmpMaxDepth,                4,        2,     8,     1);
TUNE_PARAM(LmpBase,                    4,        0,     16,    2);
TUNE_PARAM(LmpMult,                    2,        1,     6,     1);

// SEE pruning (quiet moves)
TUNE_PARAM(SeePruningMargin,           35,       10,    80,    5);
TUNE_PARAM(SeePruningMaxDepth,         6,        3,     10,    1);

// History pruning
TUNE_PARAM(HistoryPruningMargin,       1000,     300,   2500,  100);
TUNE_PARAM(HistoryPruningMaxDepth,     4,        2,     8,     1);

// Late move reduction
TUNE_PARAM(LmrMinDepth,                3,        2,     6,     1);
TUNE_PARAM(LmrMinMoveCount,            2,        1,     4,     1);
TUNE_PARAM(LmrBase100,                 50,       0,     150,   10);
TUNE_PARAM(LmrDivisor100,              200,      100,   400,   15);
TUNE_PARAM(LmrPvReduction,             1,        0,     2,     1);
TUNE_PARAM(LmrHistoryThreshold,        1000,     200,   3000,  150);

// Aspiration window
TUNE_PARAM(AspirationWindow,           50,       10,    100,   5);

// History heuristic
TUNE_PARAM(HistoryMalusDivisor,        2,        1,     4,     1);
TUNE_PARAM(HistoryGravityThreshold,    32768,    8192,  65536, 4096);
TUNE_PARAM(HistoryPlyDivisor,          4,        1,     8,     1);
TUNE_PARAM(CaptureAttackerDivisor,     8,        2,     16,    1);
TUNE_PARAM(CaptureSeeDivisor,          4,        1,     8,     1);

// Killer/history aging
TUNE_PARAM(HistAgingFactorPermille,    900,      700,   999,   20);
TUNE_PARAM(HistAgingLowTimePermille,   980,      900,   999,   10);

} // namespace Sloth

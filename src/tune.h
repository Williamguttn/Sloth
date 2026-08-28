#ifndef TUNE_H_INCLUDED
#define TUNE_H_INCLUDED

#include <vector>

namespace Sloth {
namespace Tune {

    struct TunableParam {
        const char* name;
        int*        value;
        int         defaultValue;
        int         min;
        int         max;
        int         step;   // SPSA C_end (initial step size)
    };

    std::vector<TunableParam>& params();

    struct Registrar {
        Registrar(const char* name, int* value, int defaultValue, int min, int max, int step);
    };

    void printUCIOptions();

    bool setParam(const char* name, int value);

    void printSPSAInput();

} // namespace Tune
} // namespace Sloth

#define TUNE_PARAM(name, defaultValue, minValue, maxValue, step) \
    int name = defaultValue; \
    static Sloth::Tune::Registrar name##_registrar(#name, &name, defaultValue, minValue, maxValue, step)

namespace Sloth {

    // ---- Reverse futility / static-eval pruning ----
    extern int RfpQMargin;
    extern int RfpMargin1PerDepth;
    extern int RfpMargin2PerDepth;

    // ---- Futility pruning (main move loop) ----
    extern int FutilityMarginPerDepth;
    extern int FutilityMaxDepth;

    // ---- Razoring ----
    extern int RazorBaseMargin;
    extern int RazorMarginPerDepth;
    extern int RazorMaxDepth;

    // ---- Internal iterative reduction ----
    extern int IirMinDepth;

    // ---- ProbCut ----
    extern int ProbCutMargin;
    extern int ProbCutMinDepth;
    extern int ProbCutReduction;
    extern int ProbCutTTDepthMargin;

    // ---- Null move pruning ----
    extern int NmpMinDepth;
    extern int NmpBaseReduction;
    extern int NmpDepthBonusReduction; // extra reduction added when depth >= NmpDepthThreshold
    extern int NmpDepthThreshold;

    // ---- Late move pruning ----
    // margin(depth) = LmpBase + LmpMult * depth * depth
    extern int LmpMaxDepth;
    extern int LmpBase;
    extern int LmpMult;

    // ---- SEE pruning (quiet moves) ----
    extern int SeePruningMargin;
    extern int SeePruningMaxDepth;

    // ---- History pruning ----
    extern int HistoryPruningMargin;
    extern int HistoryPruningMaxDepth;

    // ---- Late move reduction ----
    extern int LmrMinDepth;
    extern int LmrMinMoveCount;
    extern int LmrBase100;     // formula base, scaled by 100
    extern int LmrDivisor100;  // formula divisor, scaled by 100
    extern int LmrPvReduction;
    extern int LmrHistoryThreshold;

    // ---- Aspiration window ----
    extern int AspirationWindow;

    // ---- History heuristic ----
    extern int HistoryMalusDivisor;
    extern int HistoryGravityThreshold;
    extern int HistoryPlyDivisor;
    extern int CaptureAttackerDivisor;
    extern int CaptureSeeDivisor;

    // ---- Killer/history aging ----
    extern int HistAgingFactorPermille;
    extern int HistAgingLowTimePermille;

} // namespace Sloth

#endif

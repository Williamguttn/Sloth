// time.h
#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED
#include <iostream>
#include <chrono>

static unsigned long long getTickCount() {
    return static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

namespace Sloth {
    class Time {
    public:
        bool quit = false;
        bool stopped = false;
        bool ponder = true;

        bool ponderPlayed = false;

        int movesToGo = 30;
        int moveTime = -1;
        int time = -1;
        int inc = 0;
        int startTime = 0;
        int stopTime = 0;
        int timeSet = 0;
        
        int gamePhase = 0;        // 0=opening, 1=midgame, 2=endgame
        int moveNumber = 1;
        
        float openingTimeFactor = 0.8f;    // Use less time in opening
        float midgameTimeFactor = 1.0f;    // Standard time in midgame
        float endgameTimeFactor = 1.2f;    // Use more time in endgame
        float criticalPositionFactor = 1.5f; // More time for critical positions
        
        int getTimeMs();
        void communicate();
        
        void initTimeControl(int timeLeft, int increment, int movesToTimeControl);
        int calculateMoveTime();
        void adjustTimeForPosition(bool isCritical);
        void setGamePhase(int phase);
        bool shouldStopSearch();
        void updateMoveNumber(int move);
    };
}
#endif
#include "time.h"
#include "threads.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#endif 

namespace Sloth {

int Time::getTimeMs() {
#ifdef _WIN32
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

void Time::communicate() {
    ponder = Threads::pondering.load(std::memory_order_relaxed);
    timeSet = Threads::timeControlActive.load(std::memory_order_relaxed) ? 1 : 0;
    startTime = Threads::startTimeMs.load(std::memory_order_relaxed);
    stopTime = Threads::stopTimeMs.load(std::memory_order_relaxed);

    if (timeSet == 1 && !ponder && shouldStopSearch()) {
        Threads::stopFlag.store(true, std::memory_order_relaxed);
    }
}


// Initialize time control parameters
void Time::initTimeControl(int timeLeft, int increment, int movesToTimeControl) {
    time = timeLeft;
    inc = increment;
    movesToGo = movesToTimeControl > 0 ? movesToTimeControl : 30;
    
    startTime = getTimeMs();
    
    int moveTime = calculateMoveTime();
    
    // Set the stop time
    stopTime = startTime + moveTime;
    timeSet = 1;
}

// TODO: Overcomplicated?
int Time::calculateMoveTime() {
    if (moveTime > 0) {
        return moveTime;
    }
    
    if (time <= 0) {
        return 1000;
    }
    
    float timeFactor;
    
    switch (gamePhase) {
        case 0: timeFactor = openingTimeFactor; break; // Opening
        case 1: timeFactor = midgameTimeFactor; break; // Midgame
        case 2: timeFactor = endgameTimeFactor; break; // Endgame
        default: timeFactor = 1.0f;
    }
    
    int timeForMove = 0;
    
    if (movesToGo > 0) {
        timeForMove = (int)((time * timeFactor) / (movesToGo + 5));
    } else {
        timeForMove = (int)((time * timeFactor) / 30) + (inc * 3/4);
    }
    
    // Ensure we're not using too much or too little time
    int minTime = MIN(50, time / 20);
    int maxTime = MAX(time / 2, time - 1000);
    
    timeForMove = MAX(minTime, MIN(timeForMove, maxTime));
    
    return timeForMove;
}

// Adjust time allocation for critical positions
void Time::adjustTimeForPosition(bool isCritical) {
    if (!isCritical || timeSet != 1) return;
    
    // Extend time for critical positions
    int currentTime = getTimeMs();
    int elapsed = currentTime - startTime;
    int remaining = stopTime - currentTime;
    
    if (remaining > 0) {
        // Add extra time for critical positions
        int extraTime = (int)(remaining * (criticalPositionFactor - 1.0f));
        stopTime += extraTime;
    }
}

// Set the current game phase for better time management
void Time::setGamePhase(int phase) {
    gamePhase = MAX(0, MIN(2, phase));  // Clamp between 0-2
}

// Update the move number
void Time::updateMoveNumber(int move) {
    moveNumber = move;
    
    if (moveNumber <= 10) {
        gamePhase = 0; // Opening
    } else if (moveNumber <= 40) {
        gamePhase = 1; // Midgame
    } else {
        gamePhase = 2; // Endgame
    }
}

bool Time::shouldStopSearch() {
    if (moveTime > 0) {
        return getTimeMs() >= stopTime;
    }
    
    int currentTime = getTimeMs();
    int elapsed = currentTime - startTime;
    
    if (currentTime >= stopTime) {
        return true;
    }
    
    // If we're very close to the allocated time consider stopping
    if (elapsed >= (stopTime - startTime) * 0.95) {
        return true;
    }
    
    // More aggressive time management if the time is less than 5 seconds
    if (time > 0 && time < 5000 && elapsed > (stopTime - startTime) * 0.5) {
        return true;
    }
    
    return false;
}

} // namespace Sloth
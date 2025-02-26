/*
	Thanks to VICE by Bluefever Software
*/

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

		int movesToGo = 30;
		int moveTime = -1;
		int time = -1;
		int inc = 0;
		int startTime = 0;
		int stopTime = 0;
		int timeSet = 0;

		int getTimeMs();
		int inputWaiting();
		
		void readInput();
		void communicate();
	};
}
#endif


#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>


#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#endif 




	int Time::getTimeMs() {
	#ifdef _WIN32
		return GetTickCount();
	#else
		struct timeval tv;                             // for GCC/Clang - JA
		gettimeofday(&tv, NULL);
		return tv.tv_sec * 1000 + tv.tv_usec / 1000;
	#endif
	}

	int Time::inputWaiting() {
	#ifndef _WIN32                                    // for GCC/Clang - JA
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		struct timeval tv;
		tv.tv_sec = 0; 
		tv.tv_usec = 0;
		select(fileno(stdin) + 1, &readfds, NULL, NULL, &tv);
		return FD_ISSET(fileno(stdin), &readfds);
	#else
		static int init = 0, pipe;
		static HANDLE inh;
		DWORD dw;

		if (!init) {
			init = 1;
			inh = GetStdHandle(STD_INPUT_HANDLE);
			pipe = !GetConsoleMode(inh, &dw);
			if (!pipe) {
				SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
				FlushConsoleInputBuffer(inh);
			}
		}

		if (pipe) {
			if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
			return dw;
		} else {
			GetNumberOfConsoleInputEvents(inh, &dw);
			return dw <= 1 ? 0 : dw;
		}
	#endif
	}

	void Time::readInput() {
		char input[256] = "";

		if (inputWaiting()) {
			stopped = true;

			std::cin.getline(input, 256);
			int bytes = std::cin.gcount();

			if (bytes > 0) {
				char* endc = strchr(input, '\n');
				if (endc) *endc = 0;

				if (std::strncmp(input, "quit", 4) == 0 || std::strncmp(input, "stop", 4) == 0) {
					quit = true;
				}
			}
		}
	}

	void Time::communicate() {
		if (timeSet == 1 && getTimeMs() > stopTime) {
			stopped = true;
		}

		readInput();
	}




#pragma once

#include "position.h"
#include <time.h>
#include <stdint.h>

namespace Sloth {
	namespace Perft {
		static int getTimeMs() {
			return getTickCount();
		}

		extern uint64_t nodes;
		extern void perft(int depth, Position& pos);
		void perftTest(int depth, Position& pos);
	}
}


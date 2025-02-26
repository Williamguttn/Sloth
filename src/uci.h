#ifndef UCI_H_INCLUDED
#define UCI_H_INCLUDED

#include "position.h"
//#include "threads.h"
#include "types.h"
//#include "search2.h"

namespace Sloth {
	namespace UCI {

		int parseMove(Position& pos, const char* moveString); // parse move string like e7e8q
		void parsePosition(Position& pos, const char* command);
		void parseGo(Position& pos, const char* command);

		void loop();
	}
}

#endif
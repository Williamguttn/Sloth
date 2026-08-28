#include <algorithm>

#include "evaluate.h"

#include "bitboards.h"
#include "piece.h"
#include "position.h"
#include "magic.h"
#include "types.h"

namespace Sloth {
	inline int Eval::evaluate(Position& pos) {
		return nn_evaluate(pos.nnue_acc, pos.sideToMove);
	}
}
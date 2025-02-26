#include "piece.h"

namespace Sloth {
	int Piece::charToPiece(char pieceChar) {
        switch (pieceChar) {
            case 'P': return Pieces::P;
            case 'N': return Pieces::N;
            case 'B': return Pieces::B;
            case 'R': return Pieces::R;
            case 'Q': return Pieces::Q;
            case 'K': return Pieces::K;
            case 'p': return Pieces::p;
            case 'n': return Pieces::n;
            case 'b': return Pieces::b;
            case 'r': return Pieces::r;
            case 'q': return Pieces::q;
            case 'k': return Pieces::k;
            default:  return Pieces::emptyPiece; // Ensure a valid return for unrecognized characters
        }
	}
}
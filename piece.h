#ifndef PIECE_H_INCLUDED
#define PIECE_H_INCLUDED

namespace Sloth {
	namespace Piece {

		const char asciiPieces[13] = "PNBRQKpnbrqk";

		enum {rook, bishop};

		// Encoded pieces
		enum Pieces : int {P, N, B, R, Q, K, p, n, b, r, q, k,emptyPiece};

		int charToPiece(char pieceChar);
	}
}

#endif
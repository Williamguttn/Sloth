#ifndef NNUE_H_INCLUDED
#define NNUE_H_INCLUDED

#include <inttypes.h>

// (768 -> NN_HIDDEN)x2 -> NN_OUTPUT_BUCKETS, output bucket selected by piece count on board

#ifndef NN_HIDDEN
#define NN_HIDDEN 512
#endif
#ifndef NN_OUTPUT_BUCKETS
#define NN_OUTPUT_BUCKETS 1 // defaults to 8 in build files
#endif
#define NN_QA 255
#define NN_QB 64
#define NN_SCALE 400

typedef int16_t NN_Accumulator[2][NN_HIDDEN];

int nn_load(const char* filename);

void nn_init_accumulator(NN_Accumulator acc);

void nn_add_piece(NN_Accumulator acc, int piece_type, int piece_color, int sq);
void nn_del_piece(NN_Accumulator acc, int piece_type, int piece_color, int sq);
void nn_mov_piece(NN_Accumulator acc, int piece_type, int piece_color, int from, int to);
void nn_capture_piece(NN_Accumulator acc, int piece_type, int piece_color, int from, int to, int captured_type);

int nn_evaluate(NN_Accumulator acc, int sideToMove, int pieceCount);

#endif // NNUE_H_INCLUDED

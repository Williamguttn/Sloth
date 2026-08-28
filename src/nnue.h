#ifndef NNUE_H_INCLUDED
#define NNUE_H_INCLUDED

#include <inttypes.h>

// (768 -> NN_HIDDEN)x2 -> 1

#define NN_HIDDEN 512
#define NN_QA 255
#define NN_QB 64
#define NN_SCALE 400

typedef int16_t NN_Accumulator[2][NN_HIDDEN];

int nn_load(const char* filename);

void nn_init_accumulator(NN_Accumulator acc);

void nn_add_piece(NN_Accumulator acc, int piece_type, int piece_color, int sq);
void nn_del_piece(NN_Accumulator acc, int piece_type, int piece_color, int sq);
void nn_mov_piece(NN_Accumulator acc, int piece_type, int piece_color, int from, int to);

int nn_evaluate(NN_Accumulator acc, int sideToMove);

#endif // NNUE_H_INCLUDED

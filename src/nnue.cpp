#include "nnue.h"

#include <stdio.h>
#include <string.h>

#ifdef EVALFILE_EMBEDDED
#include "embedded_net.cpp"   // gEmbeddedNNUEData[], gEmbeddedNNUESize
#endif

#pragma pack(push, 1)
struct NN_Network { // (768 -> NN_HIDDEN)x2 -> 1
    int16_t featureWeights[768][NN_HIDDEN];
    int16_t featureBias[NN_HIDDEN];
    int16_t outputWeights[2 * NN_HIDDEN];
    int16_t outputBias;
};
#pragma pack(pop)

static NN_Network nn_net;

int nn_load(const char* filename) {
    memset(&nn_net, 0, sizeof(nn_net));

#ifdef EVALFILE_EMBEDDED
    (void)filename; // embedded net always wins when compiled with EVALFILE=
    if (gEmbeddedNNUESize < sizeof(nn_net)) return -1;
    memcpy(&nn_net, gEmbeddedNNUEData, sizeof(nn_net));
    return 0;
#else
    FILE* file = fopen(filename, "rb");
    if (file == NULL) return -1;

    size_t read = fread(&nn_net, sizeof(nn_net), 1, file);
    fclose(file);

    if (read == 0) return -1;
    return 0;
#endif
}

void nn_init_accumulator(NN_Accumulator acc) {
    for (int i = 0; i < NN_HIDDEN; i++) {
        acc[0][i] = nn_net.featureBias[i];
        acc[1][i] = nn_net.featureBias[i];
    }
}

static inline int nn_white_index(int piece_type, int piece_color, int sq) {
    return 64 * piece_type + sq + (piece_color == 0 ? 0 : 384);
}

static inline int nn_black_index(int piece_type, int piece_color, int sq) {
    return 64 * piece_type + (sq ^ 56) + (piece_color == 0 ? 384 : 0);
}

void nn_add_piece(NN_Accumulator acc, int piece_type, int piece_color, int sq) {
    const int16_t* wcol = nn_net.featureWeights[nn_white_index(piece_type, piece_color, sq)];
    const int16_t* bcol = nn_net.featureWeights[nn_black_index(piece_type, piece_color, sq)];

    for (int i = 0; i < NN_HIDDEN; i++) {
        acc[0][i] += wcol[i];
        acc[1][i] += bcol[i];
    }
}

void nn_del_piece(NN_Accumulator acc, int piece_type, int piece_color, int sq) {
    const int16_t* wcol = nn_net.featureWeights[nn_white_index(piece_type, piece_color, sq)];
    const int16_t* bcol = nn_net.featureWeights[nn_black_index(piece_type, piece_color, sq)];

    for (int i = 0; i < NN_HIDDEN; i++) {
        acc[0][i] -= wcol[i];
        acc[1][i] -= bcol[i];
    }
}

void nn_mov_piece(NN_Accumulator acc, int piece_type, int piece_color, int from, int to) {
    nn_del_piece(acc, piece_type, piece_color, from);
    nn_add_piece(acc, piece_type, piece_color, to);
}

// Squared Clipped ReLU: clamp(x, 0, QA)^2
static inline int32_t nn_screlu(int16_t x) {
    int32_t y = x;
    if (y < 0) y = 0;
    if (y > NN_QA) y = NN_QA;
    return y * y;
}

int nn_evaluate(NN_Accumulator acc, int sideToMove) {
    const int16_t* us = acc[sideToMove];
    const int16_t* them = acc[1 - sideToMove];

    int64_t output = 0;

    for (int i = 0; i < NN_HIDDEN; i++)
        output += (int64_t)nn_screlu(us[i]) * (int64_t)nn_net.outputWeights[i];

    for (int i = 0; i < NN_HIDDEN; i++)
        output += (int64_t)nn_screlu(them[i]) * (int64_t)nn_net.outputWeights[NN_HIDDEN + i];

    // reduce QA*QA*QB -> QA*QB, add bias, apply eval scale, remove quantisation.
    output /= NN_QA;
    output += nn_net.outputBias;
    output *= NN_SCALE;
    output /= (int64_t)NN_QA * (int64_t)NN_QB;

    return (int)output;
}

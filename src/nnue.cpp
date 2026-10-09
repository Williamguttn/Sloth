#include "nnue.h"

#include <stdio.h>
#include <string.h>

#ifdef EVALFILE_EMBEDDED
#include "embedded_net.cpp"   // gEmbeddedNNUEData[], gEmbeddedNNUESize
#endif

#if defined(NN_NO_SIMD)
#define NN_SIMD 0
#elif defined(__AVX2__)
#include <immintrin.h>
#define NN_SIMD 1
#define NN_LANES 16
typedef __m256i nn_vec;
static inline nn_vec v_load(const int16_t* p) { return _mm256_loadu_si256((const __m256i*)p); }
static inline void v_store(int16_t* p, nn_vec v) { _mm256_storeu_si256((__m256i*)p, v); }
static inline nn_vec v_add16(nn_vec a, nn_vec b) { return _mm256_add_epi16(a, b); }
static inline nn_vec v_sub16(nn_vec a, nn_vec b) { return _mm256_sub_epi16(a, b); }
static inline nn_vec v_clamp(nn_vec a) { return _mm256_min_epi16(_mm256_max_epi16(a, _mm256_setzero_si256()), _mm256_set1_epi16(NN_QA)); }
static inline nn_vec v_mullo16(nn_vec a, nn_vec b) { return _mm256_mullo_epi16(a, b); }
static inline nn_vec v_madd16(nn_vec a, nn_vec b) { return _mm256_madd_epi16(a, b); }
static inline nn_vec v_add32(nn_vec a, nn_vec b) { return _mm256_add_epi32(a, b); }
static inline nn_vec v_zero() { return _mm256_setzero_si256(); }
static inline int64_t v_hsum32(nn_vec v) {
    int32_t lanes[8];
    _mm256_storeu_si256((__m256i*)lanes, v);
    int64_t s = 0;
    for (int i = 0; i < 8; i++) s += lanes[i];
    return s;
}
#elif defined(__SSE2__)
#include <emmintrin.h>
#define NN_SIMD 1
#define NN_LANES 8
typedef __m128i nn_vec;
static inline nn_vec v_load(const int16_t* p) { return _mm_loadu_si128((const __m128i*)p); }
static inline void v_store(int16_t* p, nn_vec v) { _mm_storeu_si128((__m128i*)p, v); }
static inline nn_vec v_add16(nn_vec a, nn_vec b) { return _mm_add_epi16(a, b); }
static inline nn_vec v_sub16(nn_vec a, nn_vec b) { return _mm_sub_epi16(a, b); }
static inline nn_vec v_clamp(nn_vec a) { return _mm_min_epi16(_mm_max_epi16(a, _mm_setzero_si128()), _mm_set1_epi16(NN_QA)); }
static inline nn_vec v_mullo16(nn_vec a, nn_vec b) { return _mm_mullo_epi16(a, b); }
static inline nn_vec v_madd16(nn_vec a, nn_vec b) { return _mm_madd_epi16(a, b); }
static inline nn_vec v_add32(nn_vec a, nn_vec b) { return _mm_add_epi32(a, b); }
static inline nn_vec v_zero() { return _mm_setzero_si128(); }
static inline int64_t v_hsum32(nn_vec v) {
    int32_t lanes[4];
    _mm_storeu_si128((__m128i*)lanes, v);
    return (int64_t)lanes[0] + lanes[1] + lanes[2] + lanes[3];
}
#else
#define NN_SIMD 0
#endif

#if NN_SIMD
static_assert(NN_HIDDEN % NN_LANES == 0, "NN_HIDDEN must be a multiple of the SIMD width");
#endif

#pragma pack(push, 1)
struct NN_Network { // (768 -> NN_HIDDEN)x2 -> NN_OUTPUT_BUCKETS
    int16_t featureWeights[768][NN_HIDDEN];
    int16_t featureBias[NN_HIDDEN];
    int16_t outputWeights[NN_OUTPUT_BUCKETS][2 * NN_HIDDEN];
    int16_t outputBias[NN_OUTPUT_BUCKETS];
};
#pragma pack(pop)

static NN_Network nn_net;

static constexpr size_t NN_FILE_SIZE_PADDED = (sizeof(NN_Network) + 63) / 64 * 64;

static bool nn_size_matches(size_t size) {
    return size == sizeof(NN_Network) || size == NN_FILE_SIZE_PADDED;
}

#ifdef EVALFILE_EMBEDDED
static_assert(sizeof(gEmbeddedNNUEData) == sizeof(NN_Network) || sizeof(gEmbeddedNNUEData) == NN_FILE_SIZE_PADDED,
              "embedded net does not match NN_HIDDEN / NN_OUTPUT_BUCKETS -- pass the right -DNN_OUTPUT_BUCKETS=N / -DNN_HIDDEN=N");
#endif

static bool nn_simd_output_ok = false;

static void nn_after_load() {
    nn_simd_output_ok = NN_SIMD;
    for (int b = 0; b < NN_OUTPUT_BUCKETS; b++)
        for (int i = 0; i < 2 * NN_HIDDEN; i++) {
            int w = nn_net.outputWeights[b][i];
            if (w > 128 || w < -128) nn_simd_output_ok = false;
        }
}

int nn_load(const char* filename) {
    memset(&nn_net, 0, sizeof(nn_net));

#ifdef EVALFILE_EMBEDDED
    (void)filename; // embedded net always wins when compiled with EVALFILE=
    if (!nn_size_matches(gEmbeddedNNUESize)) return -1;
    memcpy(&nn_net, gEmbeddedNNUEData, sizeof(nn_net));
    nn_after_load();
    return 0;
#else
    FILE* file = fopen(filename, "rb");
    if (file == NULL) return -1;

    fseek(file, 0, SEEK_END);
    const long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (fileSize < 0 || !nn_size_matches((size_t)fileSize)) {
        fclose(file);
        return -1;
    }

    size_t read = fread(&nn_net, sizeof(nn_net), 1, file);
    fclose(file);

    if (read == 0) return -1;
    nn_after_load();
    return 0;
#endif
}

void nn_init_accumulator(NN_Accumulator acc) {
    memcpy(acc[0], nn_net.featureBias, sizeof(nn_net.featureBias));
    memcpy(acc[1], nn_net.featureBias, sizeof(nn_net.featureBias));
}

static inline int nn_white_index(int piece_type, int piece_color, int sq) {
    return 64 * piece_type + sq + (piece_color == 0 ? 0 : 384);
}

static inline int nn_black_index(int piece_type, int piece_color, int sq) {
    return 64 * piece_type + (sq ^ 56) + (piece_color == 0 ? 384 : 0);
}

static inline const int16_t* nn_col(int index) {
    return nn_net.featureWeights[index];
}

// acc += add
static inline void nn_acc_add(int16_t* acc, const int16_t* add) {
#if NN_SIMD
    for (int i = 0; i < NN_HIDDEN; i += NN_LANES)
        v_store(acc + i, v_add16(v_load(acc + i), v_load(add + i)));
#else
    for (int i = 0; i < NN_HIDDEN; i++) acc[i] += add[i];
#endif
}

// acc -= sub
static inline void nn_acc_sub(int16_t* acc, const int16_t* sub) {
#if NN_SIMD
    for (int i = 0; i < NN_HIDDEN; i += NN_LANES)
        v_store(acc + i, v_sub16(v_load(acc + i), v_load(sub + i)));
#else
    for (int i = 0; i < NN_HIDDEN; i++) acc[i] -= sub[i];
#endif
}

// acc += add - sub
static inline void nn_acc_add_sub(int16_t* acc, const int16_t* add, const int16_t* sub) {
#if NN_SIMD
    for (int i = 0; i < NN_HIDDEN; i += NN_LANES)
        v_store(acc + i, v_sub16(v_add16(v_load(acc + i), v_load(add + i)), v_load(sub + i)));
#else
    for (int i = 0; i < NN_HIDDEN; i++) acc[i] += add[i] - sub[i];
#endif
}

// acc += add - sub1 - sub2
static inline void nn_acc_add_sub_sub(int16_t* acc, const int16_t* add, const int16_t* sub1, const int16_t* sub2) {
#if NN_SIMD
    for (int i = 0; i < NN_HIDDEN; i += NN_LANES) {
        nn_vec v = v_add16(v_load(acc + i), v_load(add + i));
        v = v_sub16(v, v_load(sub1 + i));
        v_store(acc + i, v_sub16(v, v_load(sub2 + i)));
    }
#else
    for (int i = 0; i < NN_HIDDEN; i++) acc[i] += add[i] - sub1[i] - sub2[i];
#endif
}

void nn_add_piece(NN_Accumulator acc, int piece_type, int piece_color, int sq) {
    nn_acc_add(acc[0], nn_col(nn_white_index(piece_type, piece_color, sq)));
    nn_acc_add(acc[1], nn_col(nn_black_index(piece_type, piece_color, sq)));
}

void nn_del_piece(NN_Accumulator acc, int piece_type, int piece_color, int sq) {
    nn_acc_sub(acc[0], nn_col(nn_white_index(piece_type, piece_color, sq)));
    nn_acc_sub(acc[1], nn_col(nn_black_index(piece_type, piece_color, sq)));
}

void nn_mov_piece(NN_Accumulator acc, int piece_type, int piece_color, int from, int to) {
    nn_acc_add_sub(acc[0], nn_col(nn_white_index(piece_type, piece_color, to)),
                           nn_col(nn_white_index(piece_type, piece_color, from)));
    nn_acc_add_sub(acc[1], nn_col(nn_black_index(piece_type, piece_color, to)),
                           nn_col(nn_black_index(piece_type, piece_color, from)));
}

void nn_capture_piece(NN_Accumulator acc, int piece_type, int piece_color, int from, int to, int captured_type) {
    const int enemy = piece_color ^ 1;
    nn_acc_add_sub_sub(acc[0], nn_col(nn_white_index(piece_type, piece_color, to)),
                               nn_col(nn_white_index(piece_type, piece_color, from)),
                               nn_col(nn_white_index(captured_type, enemy, to)));
    nn_acc_add_sub_sub(acc[1], nn_col(nn_black_index(piece_type, piece_color, to)),
                               nn_col(nn_black_index(piece_type, piece_color, from)),
                               nn_col(nn_black_index(captured_type, enemy, to)));
}

// Squared Clipped ReLU: clamp(x, 0, QA)^2
static inline int32_t nn_screlu(int16_t x) {
    int32_t y = x;
    if (y < 0) y = 0;
    if (y > NN_QA) y = NN_QA;
    return y * y;
}

// sum_i screlu(x[i]) * w[i], exact (same value as the scalar loop)
static inline int64_t nn_screlu_dot(const int16_t* x, const int16_t* w) {
#if NN_SIMD
    if (nn_simd_output_ok) {
        int64_t total = 0;
        nn_vec sum = v_zero();
        int steps = 0;
        for (int i = 0; i < NN_HIDDEN; i += NN_LANES) {
            nn_vec v = v_clamp(v_load(x + i));
            nn_vec vw = v_mullo16(v, v_load(w + i));
            sum = v_add32(sum, v_madd16(vw, v));
            if (++steps == 64) {
                total += v_hsum32(sum);
                sum = v_zero();
                steps = 0;
            }
        }
        return total + v_hsum32(sum);
    }
#endif
    int64_t total = 0;
    for (int i = 0; i < NN_HIDDEN; i++)
        total += (int64_t)nn_screlu(x[i]) * (int64_t)w[i];
    return total;
}

static inline int nn_output_bucket(int pieceCount) {
    const int divisor = (32 + NN_OUTPUT_BUCKETS - 1) / NN_OUTPUT_BUCKETS;
    int bucket = (pieceCount - 2) / divisor;
    if (bucket < 0) bucket = 0;
    if (bucket >= NN_OUTPUT_BUCKETS) bucket = NN_OUTPUT_BUCKETS - 1;
    return bucket;
}

int nn_evaluate(NN_Accumulator acc, int sideToMove, int pieceCount) {
    const int bucket = nn_output_bucket(pieceCount);
    const int16_t* outputWeights = nn_net.outputWeights[bucket];

    int64_t output = nn_screlu_dot(acc[sideToMove], outputWeights)
                   + nn_screlu_dot(acc[1 - sideToMove], outputWeights + NN_HIDDEN);

    // reduce QA*QA*QB -> QA*QB, add bias, apply eval scale, remove quantisation.
    output /= NN_QA;
    output += nn_net.outputBias[bucket];
    output *= NN_SCALE;
    output /= (int64_t)NN_QA * (int64_t)NN_QB;

    return (int)output;
}

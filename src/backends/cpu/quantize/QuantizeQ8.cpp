/*
 * This code is based on ggml(https://github.com/ggerganov/ggml),
 * please see https://github.com/ggerganov/ggml/blob/master/src/ggml.c
 * ggml is licensed under MIT Copyright (c) 2022 Georgi Gerganov:
 *
 * MIT License
 * Copyright (c) 2022 Georgi Gerganov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "QuantizeQ8.hpp"
#include "Types.hpp"

void quantize_row_q8_0_reference( float * __restrict x, block_q8_0 * __restrict y, int k) {
    assert(k % QK8_0 == 0);
    const int nb = k / QK8_0;

    for (int i = 0; i < nb; i++) {
        float amax = 0.0f; // absolute max

        for (int j = 0; j < QK8_0; j++) {
            const float v = x[i*QK8_0 + j];
            amax = MAX(amax, fabsf(v));
        }

        const float d = amax / ((1 << 7) - 1);
        const float id = d ? 1.0f/d : 0.0f;

        y[i].d = MLLM_FP32_TO_FP16(d);

        for (int j = 0; j < QK8_0; ++j) {
            const float x0 = x[i*QK8_0 + j]*id;

            y[i].qs[j] = roundf(x0);
        }
    }
}

void quantize_row_q8_0(const float * __restrict x, void * __restrict vy, int k) {
    assert(QK8_0 == 32);
    assert(k % QK8_0 == 0);
    const int nb = k / QK8_0;

    block_q8_0 * __restrict y = (block_q8_0 *)vy;

#if defined(__ARM_NEON)
    for (int i = 0; i < nb; i++) {
        float32x4_t srcv [8];
        float32x4_t asrcv[8];
        float32x4_t amaxv[8];

        for (int j = 0; j < 8; j++) srcv[j]  = vld1q_f32(x + i*32 + 4*j);
        for (int j = 0; j < 8; j++) asrcv[j] = vabsq_f32(srcv[j]);

        for (int j = 0; j < 4; j++) amaxv[2*j] = vmaxq_f32(asrcv[2*j], asrcv[2*j+1]);
        for (int j = 0; j < 2; j++) amaxv[4*j] = vmaxq_f32(amaxv[4*j], amaxv[4*j+2]);
        for (int j = 0; j < 1; j++) amaxv[8*j] = vmaxq_f32(amaxv[8*j], amaxv[8*j+4]);

        const float amax = vmaxvq_f32(amaxv[0]);

        const float d = amax / ((1 << 7) - 1);
        const float id = d ? 1.0f/d : 0.0f;

        y[i].d = MLLM_FP32_TO_FP16(d);

        for (int j = 0; j < 8; j++) {
            const float32x4_t v  = vmulq_n_f32(srcv[j], id);
            const int32x4_t   vi = vcvtnq_s32_f32(v);

            y[i].qs[4*j + 0] = vgetq_lane_s32(vi, 0);
            y[i].qs[4*j + 1] = vgetq_lane_s32(vi, 1);
            y[i].qs[4*j + 2] = vgetq_lane_s32(vi, 2);
            y[i].qs[4*j + 3] = vgetq_lane_s32(vi, 3);
        }
    }
#elif defined(__AVX2__)
    for (int i = 0; i < nb; i++) {
        // Load elements into 4 AVX vectors
        __m256 v0 = _mm256_loadu_ps( x );
        __m256 v1 = _mm256_loadu_ps( x + 8 );
        __m256 v2 = _mm256_loadu_ps( x + 16 );
        __m256 v3 = _mm256_loadu_ps( x + 24 );
        x += 32;

        // Compute max(abs(e)) for the block
        const __m256 signBit = _mm256_set1_ps( -0.0f );
        __m256 maxAbs = _mm256_andnot_ps( signBit, v0 );
        maxAbs = _mm256_max_ps( maxAbs, _mm256_andnot_ps( signBit, v1 ) );
        maxAbs = _mm256_max_ps( maxAbs, _mm256_andnot_ps( signBit, v2 ) );
        maxAbs = _mm256_max_ps( maxAbs, _mm256_andnot_ps( signBit, v3 ) );

        __m128 max4 = _mm_max_ps( _mm256_extractf128_ps( maxAbs, 1 ), _mm256_castps256_ps128( maxAbs ) );
        max4 = _mm_max_ps( max4, _mm_movehl_ps( max4, max4 ) );
        max4 = _mm_max_ss( max4, _mm_movehdup_ps( max4 ) );
        const float maxScalar = _mm_cvtss_f32( max4 );

        // Quantize these floats
        const float d = maxScalar / 127.f;
        y[i].d = MLLM_FP32_TO_FP16(d);
        const float id = ( maxScalar != 0.0f ) ? 127.f / maxScalar : 0.0f;
        const __m256 mul = _mm256_set1_ps( id );

        // Apply the multiplier
        v0 = _mm256_mul_ps( v0, mul );
        v1 = _mm256_mul_ps( v1, mul );
        v2 = _mm256_mul_ps( v2, mul );
        v3 = _mm256_mul_ps( v3, mul );

        // Round to nearest integer
        v0 = _mm256_round_ps( v0, _MM_ROUND_NEAREST );
        v1 = _mm256_round_ps( v1, _MM_ROUND_NEAREST );
        v2 = _mm256_round_ps( v2, _MM_ROUND_NEAREST );
        v3 = _mm256_round_ps( v3, _MM_ROUND_NEAREST );

        // Convert floats to integers
        __m256i i0 = _mm256_cvtps_epi32( v0 );
        __m256i i1 = _mm256_cvtps_epi32( v1 );
        __m256i i2 = _mm256_cvtps_epi32( v2 );
        __m256i i3 = _mm256_cvtps_epi32( v3 );

#if defined(__AVX2__)
        // Convert int32 to int16
        i0 = _mm256_packs_epi32( i0, i1 );	// 0, 1, 2, 3,  8, 9, 10, 11,  4, 5, 6, 7, 12, 13, 14, 15
        i2 = _mm256_packs_epi32( i2, i3 );	// 16, 17, 18, 19,  24, 25, 26, 27,  20, 21, 22, 23, 28, 29, 30, 31
                                         // Convert int16 to int8
        i0 = _mm256_packs_epi16( i0, i2 );	// 0, 1, 2, 3,  8, 9, 10, 11,  16, 17, 18, 19,  24, 25, 26, 27,  4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31

        // We got our precious signed bytes, but the order is now wrong
        // These AVX2 pack instructions process 16-byte pieces independently
        // The following instruction is fixing the order
        const __m256i perm = _mm256_setr_epi32( 0, 4, 1, 5, 2, 6, 3, 7 );
        i0 = _mm256_permutevar8x32_epi32( i0, perm );

        _mm256_storeu_si256((__m256i *)y[i].qs, i0);
#else
        // Since we don't have in AVX some necessary functions,
        // we split the registers in half and call AVX2 analogs from SSE
        __m128i ni0 = _mm256_castsi256_si128( i0 );
        __m128i ni1 = _mm256_extractf128_si256( i0, 1);
        __m128i ni2 = _mm256_castsi256_si128( i1 );
        __m128i ni3 = _mm256_extractf128_si256( i1, 1);
        __m128i ni4 = _mm256_castsi256_si128( i2 );
        __m128i ni5 = _mm256_extractf128_si256( i2, 1);
        __m128i ni6 = _mm256_castsi256_si128( i3 );
        __m128i ni7 = _mm256_extractf128_si256( i3, 1);

        // Convert int32 to int16
        ni0 = _mm_packs_epi32( ni0, ni1 );
        ni2 = _mm_packs_epi32( ni2, ni3 );
        ni4 = _mm_packs_epi32( ni4, ni5 );
        ni6 = _mm_packs_epi32( ni6, ni7 );
        // Convert int16 to int8
        ni0 = _mm_packs_epi16( ni0, ni2 );
        ni4 = _mm_packs_epi16( ni4, ni6 );

        _mm_storeu_si128((__m128i *)(y[i].qs +  0), ni0);
        _mm_storeu_si128((__m128i *)(y[i].qs + 16), ni4);
#endif
    }
#else
    // scalar
    quantize_row_q8_0_reference(x, y, k);
#endif
}



void dequantize_row_q8_0(const void * __restrict vx, float * __restrict y, int k) {
    static const int qk = QK8_0;

    assert(k % qk == 0);

    const int nb = k / qk;

    const block_q8_0 * __restrict x = (block_q8_0 *)vx;

    for (int i = 0; i < nb; i++) {
        const float d = MLLM_FP16_TO_FP32(x[i].d);

        for (int j = 0; j < qk; ++j) {
            y[i*qk + j] = x[i].qs[j]*d;
        }
    }
}




//===================================== Q8_K ==============================================

void quantize_row_q8_K_reference(const float * __restrict x, block_q8_K * __restrict y, int k) {
    assert(k % QK_K == 0);
    const int nb = k / QK_K;

    for (int i = 0; i < nb; i++) {

        float max = 0;
        float amax = 0;
        for (int j = 0; j < QK_K; ++j) {
            float ax = fabsf(x[j]);
            if (ax > amax) {
                amax = ax; max = x[j];
            }
        }
        if (amax == 0.0F) {
            y[i].d = 0;
            memset(y[i].qs, 0, QK_K);
            x += QK_K;
            continue;
        }
        const float iscale = -128.F/max;
        for (int j = 0; j < QK_K; ++j) {
            int v = nearest_int(iscale*x[j]);
            y[i].qs[j] = MIN(127, v);
        }
        for (int j = 0; j < QK_K/16; ++j) {
            int sum = 0;
            for (int ii = 0; ii < 16; ++ii) {
                sum += y[i].qs[j*16 + ii];
            }
            y[i].bsums[j] = sum;
        }
        y[i].d = 1/iscale;
        x += QK_K;
    }
}

void dequantize_row_q8_K(const block_q8_K * __restrict x, float * __restrict y, int k) {
    assert(k % QK_K == 0);
    const int nb = k / QK_K;

    for (int i = 0; i < nb; i++) {
        for (int j = 0; j < QK_K; ++j) {
            *y++ = x[i].d * x[i].qs[j];
        }
    }
}

void quantize_row_q8_K(const float * __restrict x, void * __restrict y, int k) {
    quantize_row_q8_K_reference(x, (block_q8_K  *)y, k);
}

//========================== smoothquant i8 =================================
void dequantize_row_i8(const void *__restrict vx, float *__restrict y, int k, float scale) {
    const int8_t *__restrict x = (int8_t *)vx;

    for (int i = 0; i < k; i++) {
        y[i] = x[i] * scale;
    }
}

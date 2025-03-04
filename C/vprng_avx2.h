// Marc B. Reynolds, 2025
// Public Domain under http://unlicense.org, see link for details.

// broken as is..just a quick hack 
// don't quote me on this but I think (yeah..probably faster to check that type this note)
//   haswell/skylake the AES op is available on one port
//   new models on two
// 

#pragma once

#define VPRNG_NAME "vprng_aes"  // no
//#define VPRNG_STATE_EXTERNAL
#define VPRNG_MIX_EXTERNAL

#include "vprng.h"

#include <x86intrin.h>

// total junk. wouldn't want multiple loads for non-bulk production
static const __m128i key_lo = {UINT64_C(0x85a308d3243f6a88), UINT64_C(0x0370734413198a2e)};  // no
static const __m128i key_hi = {UINT64_C(0x299f31d0a4093822), UINT64_C(0xec4e6c89082efa98)};  // double no

static inline u32x8_t vprng_mix(u64x4_t x)
{
  __m256i v;
  memcpy(&v, &x, 32);

  __m128i lo = _mm256_castsi256_si128(v);
  __m128i hi = _mm256_extracti128_si256(v, 1);

  // probably 2 rounds (w different key...just speculating)
  lo = _mm_aesenc_si128(lo, key_lo);
  hi = _mm_aesenc_si128(hi, key_hi);
  lo = _mm_aesenc_si128(lo, key_lo);
  hi = _mm_aesenc_si128(hi, key_hi);

  v  = _mm256_set_m128i(hi,lo);

  memcpy(&x, &v, 32);
  return vprng_cast_u32(x);
}

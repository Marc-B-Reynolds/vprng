// Marc B. Reynolds, 2025
// Public Domain under http://unlicense.org, see link for details.

// TODO: bunch of stuff. init being one

// WARNING: This is using AES hardware instructions to form a 128-bit mixer. This does
// not make the generators cryptographically secure.

// * to maintain uniformity requires two 128-bit state blocks so the base state is a
//   128-bit Weyl sequence (period 2^128)

#pragma once

#define VPRNG_NAME "vprng_aes"
#define VPRNG_STATE_EXTERNAL
#define VPRNG_MIX_EXTERNAL
#define VPRNG_CVPRNG_3TERM

#include "vprng.h"


//***********************************************************************************
// state update

// 
// k0 = 2-Sqrt[2]          ~= 0.585786 ->
// k1 = 3-(9+Sqrt[221])/10 ~= 0.386607 ->
// k0 = 2-Sqrt[2]          ~= 0.585786
// k1 = (9+Sqrt[221])/10-2 ~= 0.386607
static const uint64_t vprng_aes_k0_hi = UINT64_C(0x95f619980c4336f7);
static const uint64_t vprng_aes_k0_lo = UINT64_C(0x4d04ec99156a82c1);
static const uint64_t vprng_aes_k1_hi = UINT64_C(0x62f8ab0b61cf22c3);
static const uint64_t vprng_aes_k1_lo = UINT64_C(0xf801c718f4ea8ab8);
static const uint64_t vprng_aes_ones  = UINT64_C(-1);

static const u64x4_t vprng_aes_add_k = {vprng_aes_k0_lo, vprng_aes_k0_hi,
					vprng_aes_k1_lo, vprng_aes_k1_hi };


// carry detection for 128-bit addition

#if defined(__AVX512VL__)

// if hardware 64-bit unsigned compares
static const u64x4_t vprng_aes_carry_k =
{
  vprng_aes_ones-vprng_aes_k0_lo, vprng_aes_ones, 
  vprng_aes_ones-vprng_aes_k1_lo, vprng_aes_ones,
};

static inline u64x4_t vprng_aes_carry(u64x4_t v)
{
  v = v > vprng_aes_carry_k;

  return  __builtin_shufflevector(v, v, 1,0,3,2);
}

#else

// with 64-bit signed comparisons only: range remap

static const u64x4_t vprng_aes_carry_k =
{
  (vprng_aes_ones>>1)-vprng_aes_k0_lo, vprng_aes_ones>>1,
  (vprng_aes_ones>>1)-vprng_aes_k1_lo, vprng_aes_ones>>1,
};

static inline u64x4_t vprng_aes_carry(u64x4_t v)
{
  i64x4_t t = vprng_cast_i64(v^UINT64_C(0x8000000000000000));
  
  t = t > vprng_cast_i64(vprng_aes_carry_k);
  t = __builtin_shufflevector(t, t, 1,0,3,2);

  memcpy(&v,&t,32);

  return v;
}

#endif

// 128-bit Weyl sequence
static inline u64x4_t vprng_state_up(u64x4_t s, __attribute__((unused)) u64x4_t i)
{
  return v + vprng_aes_add_k - vprng_aes_carry(v);
}


//***********************************************************************************
// mixer part

// have to access hardware specific operations
#if defined(__AVX2__)
#include <x86intrin.h>

typedef __m128i vprng_aes_block_t;

static inline vprng_aes_block_t vprng_aes_step(vprng_aes_block_t x, vprng_aes_block_t k)
{
  return _mm_aesenc_si128(x,k);
}

#else

#include <arm_neon.h>
#error "fill in missing bits"

typedef uint8x16_t vprng_aes_block_t;

static inline vprng_aes_block_t vprng_aes_step(vprng_aes_block_t x, vprng_aes_block_t k)
{
}

#endif

#if 0
// simde
result_.neon_u8 = veorq_u8(
			   vaesmcq_u8(vaeseq_u8(a_.neon_u8, vdupq_n_u8(0))),
			   round_key_.neon_u8);
#endif




static inline u32x8_t vprng_mix(u64x4_t x)
{
  __m256i v;
  memcpy(&v, &x, 32);

  __m128i lo = _mm256_castsi256_si128(v);
  __m128i hi = _mm256_extracti128_si256(v, 1);

  // probably 2 rounds (w different key...just speculating)
  lo = _mm_aesenc_si128(lo, key_lo);
  hi = _mm_aesenc_si128(hi, key_lo);
  lo = _mm_aesenc_si128(lo, key_hi);
  hi = _mm_aesenc_si128(hi, key_hi);

  v  = _mm256_set_m128i(hi,lo);

  memcpy(&x, &v, 32);
  return vprng_cast_u32(x);
}


#if defined(VPRNG_SELF_TEST)

// test vector: yo!

// initial stub
void carry_test(void)
{
  u64x4_t v0 = {0};
  u64x4_t v1 = {0};
  
  for(uint32_t i=0; i<0xffffff; i++) {
    v0 = next(v0);
    
    // repeat in scalar
    uint64_t r0,r2;
    
    int c1 = __builtin_add_overflow(v1[0], vprng_aes_add_k[0], &r0);
    int c3 = __builtin_add_overflow(v1[2], vprng_aes_add_k[2], &r2);
    
    v1[0] = r0;
    v1[2] = r2;
    v1[1] = v1[1] + vprng_aes_add_k[1] + (uint64_t)c1;
    v1[3] = v1[3] + vprng_aes_add_k[3] + (uint64_t)c3;
    
    if (u64x4_eq(v0,v1)) continue;

    // fix up
    printf("%2u ",i); dump(v1,v0);
  }
}


#endif

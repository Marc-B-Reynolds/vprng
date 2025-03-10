// Marc B. Reynolds, 2025
// Public Domain under http://unlicense.org, see link for details.

// example customization header for minimum targets w/hw-supported
// SIMD 64-bit products. This is a custom variant of a PCG
// which is currently using MIX14 (on all lanes...very sad).
// The combined generator becomes something like a LMX lite
// (xorshiro state update replaced with xorshift)
// or like an XorWow with but a "Type I" XorShift instead of
// multiple state word variant with a bit finalizer added.

// TODO:
// * deal with generator pos functions which will be whacked

#pragma once

#define VPRNG_NAME "vpcg"
#define VPRNG_STATE_EXTERNAL
#define VPRNG_MIX_EXTERNAL

#include "vprng.h"

// We need four different LCG multiplicative constants
// SEE: https://gist.github.com/Marc-B-Reynolds/53ecc4f9648d9ba4348403b9afd06804
// side comments are: constant in binary, pop-count, count of 1 runs and prime factorization
// * the three low bits are always '101'. This is required to minimized block-structure.
//   (https://marc-b-reynolds.github.io/math/2017/09/12/LCS.html)
// * I don't think prime factorization is useful but it's cheap to note
// * I have 6 listed but there's no specific reasoning for the selection or ordering
// * a happier mixer would be to search for 4 sets of multiplicative constants with
//   the same shift amounts.

static const u64x4_t vpcg_mul_k =
{
  UINT64_C(0xd1342543de82ef95), // 11.1...1..11.1....1..1.1.1....1111.1111.1.....1.111.11111..1.1.1 : 32 18 5*17*1277*2908441*47750621
  UINT64_C(0xaf251af3b0f025b5), // 1.1.1111..1..1.1...11.1.1111..111.11....1111......1..1.11.11.1.1 : 33 18 5*342349289*7372892873
  UINT64_C(0xb564ef22ec7aece5), // 1.11.1.1.11..1..111.1111..1...1.111.11...1111.1.111.11..111..1.1 : 37 19 3^2*13*17*37*5101*34818615649
  UINT64_C(0xf7c2ebc08f67f2b5), // 1111.11111....1.111.1.1111......1...1111.11..1111111..1.1.11.1.1 : 38 15 79*225988494748383163
//UINT64_C(0x27bb2ee687b0b0fd), // ..1..1111.111.11..1.111.111..11.1....1111.11....1.11....111111.1 : 36 15  3*954311185259313919
//UINT64_C(0x369dea0f31a53f85), // ..11.11.1..111.1111.1.1.....1111..11...11.1..1.1..1111111....1.1 : 34 16  3*5*262370600024666923
};

static inline u64x4_t vprng_state_up(u64x4_t s, u64x4_t i)
{
  return vpcg_mul_k*s+i;
}

// temp hack: see "vsplitmix.h"
static inline u32x8_t vprng_mix(vprng_unused vprng_t* prng, u64x4_t x)
{
  x ^= x >> 30; x *= UINT64_C(0x4be98134a5976fd3);
  x ^= x >> 29; x *= UINT64_C(0x3bc0993a5ad19a13);
  x ^= x >> 31;

  return vprng_cast_u32(x);
}

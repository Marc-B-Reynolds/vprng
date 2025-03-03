// Marc B. Reynolds, 2025
// Public Domain under http://unlicense.org, see link for details.

// broken as is..just a quick hack

#pragma once

#define VPRNG_NAME "vpcg32"
#define VPRNG_STATE_EXTERNAL
//#define VPRNG_MIX_EXTERNAL

#include "vprng.h"

static const u32x8_t vpcg_mul_k = {
  0x2c9277b5, // ..1.11..1..1..1..111.1111.11.1.1 : 17 10
  0x5f356495, // .1.11111..11.1.1.11..1..1..1.1.1 : 17 11
  0x01c8e815, // .......111..1...111.1......1.1.1 : 11  7
  0xae3cc725, // 1.1.111...1111..11...111..1..1.1 : 17  9
  0x9fe72885, // 1..11111111..111..1.1...1....1.1 : 17  8
  0xadb4a92d, // 1.1.11.11.11.1..1.1.1..1..1.11.1 : 17 13
  0xa13fc965, // 1.1....1..11111111..1..1.11..1.1 : 17  9
  0x8664f205, // 1....11..11..1..1111..1......1.1 : 13  8
};

static inline u64x4_t vprng_state_up(u64x4_t s, u64x4_t i)
{
  // add part is illegal ATM
  return vprng_mix_mul(s,vpcg_mul_k)+i;
}

#if 0
// temp hack: see "vsplitmix.h"
static inline u32x8_t vprng_mix(u64x4_t x)
{
  x ^= x >> 30; x *= UINT64_C(0x4be98134a5976fd3);
  x ^= x >> 29; x *= UINT64_C(0x3bc0993a5ad19a13);
  x ^= x >> 31;

  return vprng_cast_u32(x);
}
#endif

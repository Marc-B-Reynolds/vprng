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
// state update: the two 128-bit state updates walk a fixed sequence
// 

// define the two fixed additive constants
//  k0 = 2-Sqrt[2]          ~= 0.585786
//  k1 = (9+Sqrt[221])/10-2 ~= 0.386607
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
static inline u64x4_t vprng_state_up(u64x4_t u, vprng_unused u64x4_t i)
{
  // -1 in lane if there's a carry to it
  return u + vprng_aes_add_k - vprng_aes_carry(u);
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

static inline vprng_aes_block_t vprng_aes_block_0(u64x4_t x)
{
  __m256i v; memcpy(&v, &x, 32); return _mm256_castsi256_si128(v);
}

static inline vprng_aes_block_t vprng_aes_block_1(u64x4_t x)
{
  __m256i v; memcpy(&v, &x, 32); return _mm256_extracti128_si256(v,1);
}

static inline u64x4_t vprng_aes_block_merge(vprng_aes_block_t b0, vprng_aes_block_t b1)
{
  u64x4_t r;
  __m256i v = _mm256_set_m128i(b1,b0);
  memcpy(&r, &v, 32);
  return r;
}

#elif defiined(__ARM_FEATURE_CRYPTO)
// -march=armv8-a+crypto
#include <arm_neon.h>

#warning "untested & probably broken. especially block split/merge. build and run self_check_vprng_aes"

typedef uint8x16_t vprng_aes_block_t;

// adapted from: https://blog.michaelbrase.com/2018/05/08/emulating-x86-aes-intrinsics-on-armv8-a/
static inline vprng_aes_block_t vprng_aes_step(vprng_aes_block_t x, vprng_aes_block_t k)
{
  return vaesmcq_u8(vaeseq_u8(x, (uint8x16_t){})) ^ k;
}

static inline vprng_aes_block_t vprng_aes_block_0(u64x4_t v)
{
  uint64x2_t b = {v[0],v[1]}; 

  return vreinterpretq_u8_u64(b);
}

static inline vprng_aes_block_t vprng_aes_block_1(u64x4_t v)
{
  uint64x2_t b = {v[2],v[3]}; 

  return vreinterpretq_u8_u64(b);
}

static inline u32x8_t vprng_aes_block_merge(vprng_aes_block_t b1, vprng_aes_block_t b0)
{
  uint64x2_t lo = vreinterpretq_u64_u8(b0);
  uint64x2_t hi = vreinterpretq_u64_u8(b1);
  
  return (u64x4_t){ lo[0], lo[1], hi[0], hi[1] };
}

#else
#error "either no AES hardware or this file needs updating"
#endif


static inline u32x8_t vprng_mix(vprng_t* prng, u64x4_t x)
{
  u64x4_t k = vprng_inc(prng);
  
  vprng_aes_block_t v0 = vprng_aes_block_0(x);
  vprng_aes_block_t v1 = vprng_aes_block_1(x);
  vprng_aes_block_t k0 = vprng_aes_block_0(k);
  vprng_aes_block_t k1 = vprng_aes_block_1(k);

  // very ad-hoc WIP. I've pretty much only considered
  // preserving uniformity.
  v0 = vprng_aes_step(v0, k0);
  v1 = vprng_aes_step(v1, k0);
  v0 = vprng_aes_step(v0, k1);
  v1 = vprng_aes_step(v1, k1);

  return vprng_cast_u32(vprng_aes_block_merge(v0,v1));
}


//***********************************************************************************
// testing/tooling stuff below here


#if defined(VPRNG_STAT_TESTING)
#endif

// self_check routine
#if defined(VPRNG_SELF_TEST)
#define SELF_TEST

// add output test vector: yo!

bool u64x4_eq(u64x4_t a, u64x4_t b);
void dump2_u64x4(u64x4_t a, u64x4_t b);

static const u64x4_t results[] = {
  {0x54aad01ed2e6bdfe,0x10bddf59bed2e5b7,0x54aad01ed2e6bdfe,0x10bddf59bed2e5b7},
  {0xac1538885673dd9b,0xf7c8da02101ca91c,0x16eea364b65bc94e,0xc617154338262316},
  {0xf0fae1c0e2ba2f01,0x4cc542def8cd467e,0x771d479c0608a9e4,0xb71d944e94392936},
  {0x8e57b50ee3ae8d5c,0x26906a63b59cc495,0x08d786646e27fc62,0x0cbb34da05173bf6},
  {0x883cf95aa82719dc,0x11989527b57e010a,0x820af66e441e852f,0x78daf0e11553fa09},
  {0xe55a50633989ba64,0xa3d3686b4fcdfc0f,0x7cb95c206817055e,0x6342b1cbcaf4bca9},
  {0x1b1d8c4a85c58d05,0xb7e73e04038a3b54,0x259aa8ec87fe45d8,0x7753ac51a8a83301},
  {0xf04ea550a95ad42c,0x87923b4edd2c3275,0x45c686f68490f056,0xc9439e5a939125ab},
  {0xac4b4818afd848b9,0x51f70d8e76f06f7a,0x8fe3f34382d94fb3,0x5586259d553a4e45},
  {0xc75c5bc1291851cc,0x595751fe31d82420,0x7d51eb16471a61a6,0xacee17685fbd6d42},
  {0x9dc9e16f349abc49,0x7854449c89cac8e9,0xc9090f0b1e49bd71,0x202b30ab2cd224ef},
  {0x9f6a31a95df9f517,0xb556dd9b121db0f4,0xb7dd6d4cc684e982,0x70dac0ee801ef2d3},
  {0x0157031ef8ee8582,0x76bafb511e529e6e,0x42eea265f469ebc1,0x7d9b884f5ed9a144},
  {0xb0c3f03691d6f0d6,0xe5f4a042dc41a7e4,0xf30be567e98e6b79,0x9bc957b721856b11},
  {0x171475f6e10ab27c,0x7a74d84965cc5121,0x7df377e8e0698d26,0xeb3c85c0bc155f08},
  {0x7558cb10fd2bed39,0x5fbc98a810e46da0,0x7bf37166bd2915e4,0x0fbf4a1c46ec310a},
};

uint32_t output_vectors(void)
{
  test_name("test vector");
  
  vprng_t prng = {
    .state = {0},
    .inc   = {
      UINT64_C(0x62f8ab0b61cf22c3),
      UINT64_C(0x9e3779b97f4a7c15),
      UINT64_C(0x95f619980c4336f7),
      UINT64_C(0x5dfe35e13df556d7),
    }
  };

  for(uint32_t i=0; i<16; i++) {
    u64x4_t v = vprng_u64x4(&prng);

    if (u64x4_eq(v,results[i])) continue;

    printf(WARNING "??? (any internal change?):\n" ENDC);

    for(uint32_t i=0; i<16; i++) {
      u64x4_t v = vprng_u64x4(&prng);
      printf("    {0x%016" PRIx64 ",0x%016" PRIx64 ",0x%016" PRIx64 ",0x%016" PRIx64 "},\n",
	     v[0],v[1],v[2],v[3]);
      
    }
    printf("    ");
    return test_fail();
  }
  return test_pass();
}

uint32_t split_merge(void)
{
  test_name("split/merge 128x2");

  u64x4_t v0 = {0};
  u64x4_t v1 = {0};

  // longer than really needed but who cares? probably should
  // add an explict vector to test as well.
  for(uint32_t i=0; i<0xffffff; i++) {
    v0 = vprng_state_up(v0,v0);

    vprng_aes_block_t b0 = vprng_aes_block_0(v0);
    vprng_aes_block_t b1 = vprng_aes_block_1(v0);

    v1 =  vprng_aes_block_merge(b0,b1);

    if (u64x4_eq(v0,v1)) continue;

    dump2_u64x4(v0,v1);
    return test_fail();
  }
  return test_pass();
}

// check the 128-bit addtion
uint32_t carry_test(void)
{
  test_name("128-bit addition");
  
  u64x4_t v0 = {0};
  u64x4_t v1 = {0};

  // longer than really needed but who cares? probably should
  // add an explict vector to test as well.
  for(uint32_t i=0; i<0xffffff; i++) {
    v0 = vprng_state_up(v0,v0);
    
    // repeat in scalar
    uint64_t r0,r2;
    
    int c1 = __builtin_add_overflow(v1[0], vprng_aes_add_k[0], &r0);
    int c3 = __builtin_add_overflow(v1[2], vprng_aes_add_k[2], &r2);
    
    v1[0] = r0;
    v1[2] = r2;
    v1[1] = v1[1] + vprng_aes_add_k[1] + (uint64_t)c1;
    v1[3] = v1[3] + vprng_aes_add_k[3] + (uint64_t)c3;
    
    if (u64x4_eq(v0,v1)) continue;

    // add a dump if needed
    return test_fail();
  }
  return test_pass();
}


uint32_t self_test(void)
{
  uint32_t errors = 0;

  errors += output_vectors();
  errors += carry_test();
  errors += split_merge();

  return errors;
}

#endif

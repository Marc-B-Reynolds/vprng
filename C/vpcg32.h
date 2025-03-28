// Marc B. Reynolds, 2025
// Public Domain under http://unlicense.org, see link for details.

// WIP:
// * 32 bits of state is just too small to perform well at modern testing so the
//   single state generator is going to be poor at statistical randomness no matter
//   what we do.
//   * single state consistently getting sus values at 2GB and failing on 8 GB with -tf 2
//   * reducing to 1 32-bit channel then fails start happening at 1GB (note 1/8)
// * combined generator with 2-term has passed one -tf 2 up to 512GB (need to run full)
//   * combined generator needs a proper mixer.

/*
  ./makedata_vpcg32 --test-id 1 | RNG_test stdin64 -tf 2 -tlmin 2GB -tlmax 512GB -seed 1
  
RNG_test using PractRand version 0.94
vpcg32 (0.0.1) id=0 (from test-id=1)
{9e3779b99e3779b9,9e3779b99e3779b9,9e3779b99e3779b9,9e3779b99e3779b9}

RNG = RNG_stdin64, seed = 1
test set = core, folding = extra

rng=RNG_stdin64, seed=1
length= 2 gigabytes (2^31 bytes), time= 43.7 seconds
  no anomalies in 646 test result(s)

rng=RNG_stdin64, seed=1
length= 4 gigabytes (2^32 bytes), time= 86.9 seconds
  Test Name                         Raw       Processed     Evaluation
  FPF-14+6/16:all                   R=  -9.7  p =1-4.8e-9   very suspicious
  ...and 687 test result(s) without anomalies

rng=RNG_stdin64, seed=1
length= 8 gigabytes (2^33 bytes), time= 173 seconds
  Test Name                         Raw       Processed     Evaluation
  FPF-14+6/16:(0,14-0)              R=  -9.4  p =1-2.5e-8   suspicious
  FPF-14+6/16:(1,14-0)              R=  -8.0  p =1-4.1e-7   mildly suspicious
  FPF-14+6/16:(2,14-0)              R=  -7.2  p =1-2.3e-6   unusual
  FPF-14+6/16:(3,14-0)              R=  -8.9  p =1-7.5e-8   mildly suspicious
  FPF-14+6/16:all                   R= -18.4  p =1-1.2e-17    FAIL !
  ...and 725 test result(s) without anomalies
 */  


#pragma once

#define VPRNG_NAME "vpcg32"
#define VPRNG_ADDITIVE_CONSTANT_EXTERN
#define VPRNG_STATE_EXTERNAL
#define VPRNG_MIX_EXTERNAL
//#define VPRNG_CMIX_EXTERNAL  // should be disabled ATM (just for spot checking)

#include "vprng.h"

static const u32x8_t vpcg_mul_k =
{
  0x2c9277b5, // ..1.11..1..1..1..111.1111.11.1.1 : 17 10
  0x5f356495, // .1.11111..11.1.1.11..1..1..1.1.1 : 17 11
  0x01c8e815, // .......111..1...111.1......1.1.1 : 11  7
  0xae3cc725, // 1.1.111...1111..11...111..1..1.1 : 17  9
  0x9fe72885, // 1..11111111..111..1.1...1....1.1 : 17  8
  0xadb4a92d, // 1.1.11.11.11.1..1.1.1..1..1.11.1 : 17 13
  0xa13fc965, // 1.1....1..11111111..1..1.11..1.1 : 17  9
  0x8664f205, // 1....11..11..1..1111..1......1.1 : 13  8
};


// ad.hoc choices almost certainly can be improved
static const u32x8_t vpcg_mul_m0 = {0x21f0aaad,0xa52fb2cd,0x7feb352d,0x4bdc9aa5,0xac10d4eb,0xdf892d4b,0x462daaad,0x4ffcab35};
static const u32x8_t vpcg_mul_m1 = {0x735a2d97,0x551e4d49,0x846ca68b,0x2729b469,0x9d51b169,0x3c2da6b3,0x0a36c95d,0xe98db28b};

// simply eight LCGs
static inline u64x4_t vprng_state_up(u64x4_t s, u64x4_t i)
{
  u32x8_t u = vprng_cast_u32(s);

  u = vpcg_mul_k*u + vprng_cast_u32(i);

  return vprng_cast_u64(u);
}

// to preserve uniformity we have to use a 32-bit finalizer.
static inline u32x8_t vprng_mix(vprng_unused vprng_t* prng, u64x4_t x)
{
  u32x8_t u = vprng_cast_u32(x);
  
  u ^= u >> 16; u *= vpcg_mul_m0;
  u ^= u >> 15; u *= vpcg_mul_m1;
  u ^= u >> 15;

  return u;
}

#if defined(VPRNG_CMIX_EXTERNAL)

// add a real mixer for combine that operates on 64-bit chucks.
// this is just a testing hack to see how far it gets with
// just being cheap.

static inline u32x8_t cvprng_mix(vprng_unused cvprng_t* prng, u64x4_t s0, u64x4_t s1)
{
  // LOL! No.
  s0 ^= s0 >> 16;
  s0 += s1;
  
  return s0;
}

#endif


//****************************************************************************
// 32-bit PCG requires a customized additive constant method and is structured
// differently than the baseline assumption of a Weyl sequence state update
// of the 'default'.

// BROKEN HACK ATM
// Can have exact number of parameterized generators here
// single generator version? Think.

#if defined(VPRNG_IMPLEMENTATION)

#include <stdatomic.h>

static const uint32_t vpcg_internal_inc_k  = UINT32_C(0x9e3779b9);
static const uint32_t vpcg_internal_inc_i  = UINT32_C(0x144cbc89);

static _Atomic uint32_t vpcg_internal_inc_id = 1;

static inline uint32_t vpcg_pop(uint32_t x) { return (uint32_t)__builtin_popcount(x);  }

// keep the signatures the same
void     vprng_global_id_set(uint64_t id) { atomic_store(&vpcg_internal_inc_id, (uint32_t)id);   }
uint64_t vprng_global_id_get(void)        { return (uint64_t)atomic_load(&vpcg_internal_inc_id); }

// returns an additive constant for the state update.
// * produces 2069172468 accepted values w/o top 2 bit rejection.
// * produces 1569501719 accepted values with
static uint32_t vpcg_additive_next(void)
{
  uint32_t b;

  do {
    // atomically increment the global counter and
    // convert it into a candidate additive constant
    b  = atomic_fetch_add_explicit(&vpcg_internal_inc_id, 1, memory_order_relaxed);
    b  = (b<<1)|1;
    b *= vpcg_internal_inc_k;

#if 0
    // top two bit rejection. if both are zero then it's a
    // bad candidate as a LDS in isolation. But not doing
    // a Weyl sequence so I don't think this is useful
    if ((b >> (32-2))==0) continue;
#endif    

    uint32_t pop = vpcg_pop(b);
    uint32_t t   = pop - (16-4);

    if (t <= 2*8) {
      uint32_t str = vpcg_pop(b & (b ^ (b >> 1)));
      if (str >= (pop >> 2)) return b;
    }
  } while(1);
}

void vprng_init(vprng_t* prng)
{
#if !defined(VPRNG_HIGHLANDER)
  u32x8_t v;
  
  for(uint32_t i=0; i<8; i++)
    v[i] = vpcg_additive_next();
  
  prng->inc = vprng_cast_u64(v);
#endif  

  vprng_pos_init(prng);
}

#else

// fill-in

#endif

// Marc B. Reynolds, 2025
// Public Domain under http://unlicense.org, see link for details.

#pragma once

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <assert.h>

/*
  ──────────────────────────────────────────────────────────────
  Overview (vprng)

  A small SIMD PRNG core method that fits into AVX2 operations.
  Implementation uses the GCC extension vector_size to be able
  to hit multiple architectures w/o direct intrinsics or a lib.
  This extension is supported by clang. For visual c a port
  to intrinsics would be required.

  The "goal" is to be fast and not to complete with the current
  "state-of-the art". That being said it consistently passes
  PractRand at 512 gigibytes level with "no anomalies".
  (note: only 10 trials and haven't tried larger inputs)

  * produces 256 bits per call with period is 2^64
  * there are ~2^62.9519 unique generators with easy
    selection. (number is conservative. see below)
  
  Required SIMD (for "full" speed) hardware operations:
  * 64-bit addition
  * 64-bit shift by constant
  * 32-bit product (low 32-bit result)
  
  ──────────────────────────────────────────────────────────────
  Overview (cvprng)

  A combined (two state updates) generator extension of vprng.

  * extended by Brent's 2 term xorshift (adds 32 bytes of state)
  * period extends to 2^64(2^64-1) ~= 2^128
  * one addition added to result path
  * two xorshifts added to state update path
  * signficant increase in expected statistical quality
  
  ──────────────────────────────────────────────────────────────
  Usage

  write stuff here XX

  ──────────────────────────────────────────────────────────────
  Optional compile time configuation.

  There's a set of macros that can be defined to override
  default compile time choices. They must be defined prior
  to header inclusion.

  VPRNG_CVT_F64_METHOD:
  Override default method for 64-bit integer to double conversion.
    0: equivalent to uint64_t to double cast
    1: equivalent to int64_t  to double cast
    2: bit manipulation
  Choices 0 & 1 are good for when there's a matching hardware
  operation. Current autoselection is garbage.

  VPRNG_CVT_F32_METHOD:
  Same as above but for 32-bit to single conversion

  VPRNG_DISBLE_OPTIONAL_FMA: 
  If defined disables some explict FMA usage. This is performance
  only (default assumes fast FMA) and doesn't change results.
  The compiler might still produce an FMA depending on architecture
  and compiler options.

  
  ──────────────────────────────────────────────────────────────
  Design and implementation details 
  ──────────────────────────────────────────────────────────────

  State update (vprng)

  Walk four independent 64-bit integer Weyl sequences for the
  state update. Each state has a period of 2^64.
  
  ┌─────────┬─────────┬─────────┬─────────┐
  │  weyl 3 │  weyl 2 │  weyl 1 │  weyl 0 │  
  └─────────┴─────────┴─────────┴─────────┘

    t_i = weyl_i + inc_i      (store t_i)

  The incoming states are passed to the mixer

  ──────────────────────────────────────────────────────────────
  State update (cvprng)

  The additional state of the combined generator is a single
  sequence of period 2^64-1 form by an XorShift. Specifically
  the two term one originally presented by Richard Brent.
  
  ┌─────────┬─────────┬─────────┬─────────┐
  │   x3    │   x2    │   x1    │   x0    │  
  └─────────┴─────────┴─────────┴─────────┘

     x_i ^= x_i << 7
     x_i ^= x_i >> 9         (store x_i)

  The two incoming states are added and passed to the mixer.

  A compile time option (VPRNG_CVPRNG_3TERM) changes the state
  update to a standard 3-term XorShift.

  ──────────────────────────────────────────────────────────────
  Bit finalizing (mixing stage)

  For bit finalizing we want to take the incoming state (so
  the state update is independent from mixing) and pass
  through a well performing bit finalizer. The trick is
  AVX2 is pretty limited WRT fast operations that are
  useful. But we do have 32-bit products and shifts which
  allow for the standard xorshift/multiple finalizers.

  If all targets have hardware SIMD 64-bit products and
  shift (like AVX-512) then swapping to 64-bit versions
  would give us running a four wide SplitMix.
  
  Back on topic. Using 32-bit finalizers presents some
  gotchas. The period of the low 32-bits of each state is
  only 2^32 and it is also lower entropy than the high 32
  which is already super-weak. Coupling that with it's
  going to be mixed with a 32-bit function is a sadface.
  To correct this we can do the following:

     t_i ^= t_i >> 33  // t_i = {h_i,l_i} as 32-bit

  ┌────┬────┬────┬────┬────┬────┬────┬────┐
  │ h3   l3 │ h2   l2 │ h1   l1 │ h0   l0 │ 
  └────┴────┴────┴────┴────┴────┴────┴────┘

  Now the low 32-bit chunks have a period of 2^64. The
  important part is this a bijection so uniformity is
  preserved. This "could" be replaced with a
  non-bijective step that maintains uniformity but
  none come to mind that aren't multiple steps and it
  requires very careful thinking about the uniformity.

  Next is the bit finalizer proper which uses a mixed
  bit-width operations: 64-bit chunks still t_i
  and as 32-bit chucks u_i

  ┌─────────┬─────────┬─────────┬─────────┐
  │    t3   │    t2   │    t1   │    t0   │ t_i
  ├────┬────┼────┬────┼────┬────┼────┬────┤
  │ u7 │ u6 │ u5 │ u4 │ u3 │ u2 │ u1 │ u0 │ u_i
  └────┴────┴────┴────┴────┴────┴────┴────┘

     // mix_i
     t_i ^= t_i >> 16
     u_i *= m0
     t_i ^= t_i << 15
     u_i *= m1
     t_i ^= t_i >> 17

   And we're done!
       
  ┌────┬────┬────┬────┬────┬────┬────┬────┐
  │ r7 │ r6 │ r5 │ r4 │ r3 │ r2 │ r1 │ r0 │ r_i = mix_i(u_i)
  └────┴────┴────┴────┴────┴────┴────┴────┘

*/

#define VPRNG_VERSION_MAJOR    0
#define VPRNG_VERSION_MINOR    0
#define VPRNG_VERSION_REVISION 2
#define VPRNG_VERSION (((VPRNG_VERSION_MAJOR) * 1000000) + ((VPRNG_VERSION_MINOR) * 1000) + (VPRNG_VERSION_REVISION))

#define VPRNG_STRINGIFY_EX(X) #X
#define VPRNG_STRINGIFY(X) VPRNG_STRINGIFY_EX(X)
#define VPRNG_VERSION_STR  VPRNG_STRINGIFY(VPRNG_VERSION_MAJOR) "." \
                           VPRNG_STRINGIFY(VPRNG_VERSION_MAJOR) "." \
                           VPRNG_STRINGIFY(VPRNG_VERSION_REVISION)


#ifndef VPRNG_NAME
#define VPRNG_NAME "vprng"
#endif

// this should detect if we have a SIMD hardware op for
// converting an 64-bit integer into a double. The wrapper
// allows overriding the detection.
//                          
//  0: native uint
//  1: native sint
//  2: bit hack    

//   intel: _mm256_cvtepi64_pd (vcvtqq2pd,  AVX512DQ + AVX512VL)
//          _mm256_cvtepu64_pd (vcvtuqq2pd, AVX512DQ + AVX512VL)
//          so either 0 or 1
#ifndef VPRNG_CVT_F64_METHOD
#if defined(__AVX512DQ__) && defined(__AVX512VL__)
#define VPRNG_CVT_F64_METHOD 1
#else
#define VPRNG_CVT_F64_METHOD 2
#endif
#endif

// likewise for 32-bit integer to single
//   intel: _mm256_cvtepi32_ps (vcvtdq2ps, AVX2)
#ifndef VPRNG_CVT_F32_METHOD
#if defined(__AVX2__)
#define VPRNG_CVT_F32_METHOD 0
#else
#define VPRNG_CVT_F32_METHOD 2
#endif
#endif

//*******************************************************************

#define VPRNG_XS_A 10
#define VPRNG_XS_B 7
#define VPRNG_XS_C 33

#define vprng_unused __attribute__((unused))

//*******************************************************************
// Stripped down "portable" 256 bit SIMD via vector_size extension

#if !defined(SFH_SIMD_256)
typedef uint32_t u32x8_t __attribute__ ((vector_size(32)));
typedef int32_t  i32x8_t __attribute__ ((vector_size(32)));
typedef uint64_t u64x4_t __attribute__ ((vector_size(32)));
typedef int64_t  i64x4_t __attribute__ ((vector_size(32)));
typedef float    f32x8_t __attribute__ ((vector_size(32)));
typedef double   f64x4_t __attribute__ ((vector_size(32)));

static inline u32x8_t vprng_cast_u32(u64x4_t u) { u32x8_t r; memcpy(&r,&u,32); return r; }
static inline u64x4_t vprng_cast_u64(u32x8_t u) { u64x4_t r; memcpy(&r,&u,32); return r; }

static inline u32x8_t vprng_splat_u32(uint32_t x)
{
  u32x8_t r = {x,x,x,x,x,x,x,x};
  return r;
}

static inline u64x4_t vprng_splat_u64(uint64_t x)
{
  u64x4_t r = {x,x,x,x};
  return r;
}


// remaining function defs later in file
#endif


//*******************************************************************
// pay no attention to the man behind the curtain

// version by Alexander Monakov but I'm the one wanting to coerce the
// compiler into a modifed scheduling. Supporting comment:
// "to enforce scheduling, you can tie two independent dataflow chains
//  in the barrier, for instance by pretending that the barrier modifies
//  the tail of the chain computing the output (r) and a head of the chain
//  computing the state(s) simultaneously"
// rationale: I'd prefer the bit finalizing steps to be issued prior to
// the state update to reduce the latency of the result being ready.

#if defined(VPRNG_ENABLE_BARRIER)
#define vprng_result_barrier(v1,v2) do asm ("" : "+x" (v1), "+x"(v2)); while(0)
#else
#define vprng_result_barrier(v1,v2) do {} while(0)
#endif


//*******************************************************************
// core of the PRNGs defined here

#if !defined(VPRNG_HIGHLANDER)
typedef struct { u64x4_t state; u64x4_t inc;  } vprng_t;
#else
typedef struct { u64x4_t state; } vprng_t;
#endif

// allow more than single word F2 generators
#if !defined(VPRNG_SOMETHING_OR_OTHER)
#define VPRNG_STATE_WORDS 1
#endif

typedef struct { vprng_t base;  u64x4_t f2[VPRNG_STATE_WORDS]; } cvprng_t;


// 32x8 integer product
static inline u64x4_t vprng_mix_mul(u64x4_t x, u32x8_t m)
{
  return vprng_cast_u64(vprng_cast_u32(x)*m);
}

// compile time select the bit finalizer
#if !defined(VPRNG_MIX_EXTERNAL)

// newer version than initial check-in. hand refined
// constants and added an extra xorshift. This
// drastically improved SAC measures. An optimizing
// proceedure would need to be written to get another
// jump. Comparing to the 64-bit murmur style mix14
// it's not far out on the mean but need help on the
// max. These are (same for all 4 64-bit blocks)
//
//   |bias| = 0.00078181 ± 0.00059679 : max = 0.003954 (mix14)
//   |bias| = 0.00077872 ± 0.00059169 : max = 0.003340 (mix03)
// 
// current version:
//   |bias| = 0.00089735 ± 0.00081506 : max = 0.009876
//   |bias| = 0.00101875 ± 0.00115915 : max = 0.012527
//   |bias| = 0.00101763 ± 0.00106683 : max = 0.011875
//   |bias| = 0.00085375 ± 0.00079878 : max = 0.017200
//
// original version:
//   |bias| = 0.01108408 ± 0.03786583 : max = 0.523777
//   |bias| = 0.01173061 ± 0.03889093 : max = 0.524384
//   |bias| = 0.00962076 ± 0.03821139 : max = 0.572851
//   |bias| = 0.00995480 ± 0.03496292 : max = 0.500401

static const u32x8_t vprng_finalize_m0 =
{
  0b01010100111001010101100110011001,
  0b00101100100100110111011010110101,
  0b00111010100110101010100110101011,
  0b01000101000100110110100110110101,

  0b11110011010110100010110110010111,
  0b10101101101101001010100100101011,
  0b10000010110000101111110010101101,
  0b10101100010101100100101100000111,
};

static const u32x8_t vprng_finalize_m1 =
{
  0b10010011010101110110010111011101,
  0b10101101101101001010100100101101,
  0b10000010110000011111110010101101,
  0b10101100010101100100101100000111,
  
  0b01100011011010001010101010101101,
  0b10101110110001010101000101001011,
  0b10000010111000101111001010101101,
  0b01011010010101001001110100010111,
};

static inline u32x8_t vprng_mix(vprng_unused vprng_t* prng, u64x4_t x)
{
  x ^= x >> 33;
  
  x ^= x >> 16; x = vprng_mix_mul(x,vprng_finalize_m0);
  x ^= x << 16; x = vprng_mix_mul(x,vprng_finalize_m1);
  x ^= x >> 16; x = vprng_mix_mul(x,vprng_finalize_m0);

  x ^= x >> 32;
  
  return vprng_cast_u32(x);
}
#else
static inline u32x8_t vprng_mix(vprng_t* prng, u64x4_t x);
#endif

// the default cvprng uses the same finalizer as vprng
#if !defined(VPRNG_CMIX_EXTERNAL)
static inline u32x8_t cvprng_mix(vprng_unused cvprng_t* prng, u64x4_t s0, u64x4_t s1)
{
  return vprng_mix(&prng->base, s0+s1);
}
#else
static inline u32x8_t cvprng_mix(vprng_unused cvprng_t* prng, u64x4_t s0, u64x4_t s1);
#endif

// compile time select the state update
#if !defined(VPRNG_STATE_EXTERNAL)
static inline u64x4_t vprng_state_up(u64x4_t s, u64x4_t i)
{
  return s + i;
}
#else
static inline u64x4_t vprng_state_up(u64x4_t s, u64x4_t i);
#endif


// compile time select the second state update
#if !defined(VPRNG_STATE2_EXTERNAL)
#if !defined(VPRNG_CVPRNG_3TERM)
static inline u64x4_t cvprng_state_up(u64x4_t s)
{
  s ^= s << 7;
  s ^= s >> 9;

  return s;
}
#else
static inline u64x4_t cvprng_state_up(u64x4_t s)
{
  s ^= s << 10;
  s ^= s >>  7;
  s ^= s << 33;

  return s;
}
#endif
#else
static inline u64x4_t cvprng_state_up(u64x4_t s);
#endif

// 
#if !defined(VPRNG_HIGHLANDER)
static inline u64x4_t vprng_inc (vprng_t*  prng) { return prng->inc; }
static inline u64x4_t cvprng_inc(cvprng_t* prng) { return prng->base.inc; }
#else
// temp hack first pass (partly for testing)
static const u64x4_t vprng_state_inc =
{
  UINT64_C(0x62f8ab0b61cf22c3),
  UINT64_C(0x9e3779b97f4a7c15),
  UINT64_C(0x95f619980c4336f7),
  UINT64_C(0x5dfe35e13df556d7),
};

static inline u64x4_t vprng_inc (vprng_unused vprng_t*  prng) { return vprng_state_inc; }
static inline u64x4_t cvprng_inc(vrpng_unused cvprng_t* prng) { return vprng_state_inc; }
#endif

// core base generator
static inline u32x8_t vprng_u32x8(vprng_t* prng)
{
  u64x4_t s = prng->state;
  u32x8_t r = vprng_mix(prng,s);

  vprng_result_barrier(r,s);
  
  prng->state = vprng_state_up(s, vprng_inc(prng));
    
  return r;
}

// core combined generator
static inline u32x8_t cvprng_u32x8(cvprng_t* prng)
{
  u64x4_t s0 = prng->base.state;
  u64x4_t s1 = prng->f2[0];
  u32x8_t r  = cvprng_mix(prng, s0,s1);

  vprng_result_barrier(r,s1);

  prng->base.state = vprng_state_up (s0, cvprng_inc(prng));
  prng->f2[0]      = cvprng_state_up(s1);
  
  return r;
}

//*******************************************************************
// BELOW HERE IS UTILITY FUNCTIONS AND COMPILER/ARCHTIECTURE
// BOOKEEPING/SHENANIGNS
//*******************************************************************

//*******************************************************************
// Additive constant generation described in this post:
// https://marc-b-reynolds.github.io/math/2024/12/11/LCGAddConst.html
// Most of the comments have been stipped here. One difference is
// that I've flipped to using 'phi' (comment below)
//
// Don't have the exact number of generators produced. The intial window
// filter produces n = 17842322501232739958 candidates but I haven't
// bothered figuring out how this number is reduced by the bit string
// count (str < floor(pop/4)) rejection and need an exact number to be
// be able to calculate the number of generators. But:
//
// Don't think it matters much because log2(floor[n/4]) ~= 61.9519

#if defined(VPRNG_IMPLEMENTATION)

#if !defined(VPRNG_ADDITIVE_CONSTANT_EXTERN)
#include <stdatomic.h>


// 64-bit population count
static_assert(sizeof(long long int) == 8, "fixup vprng_pop");

static inline uint32_t vprng_pop(uint64_t x) { return (uint32_t)__builtin_popcountll(x); }


// differs from post which assumes a strong bit finalizer. Here
// I'm assuming it might be weak so going with an optimal 1D
// discrepancy choice.
//   magic    = frac(phi) = 1/phi = (Sqrt[5]-1)/2
//   constant = RoundToOdd[2^64*magic]
// and its mod inverse:
static const uint64_t vprng_internal_inc_k  = UINT64_C(0x9e3779b97f4a7c15);
static const uint64_t vprng_internal_inc_i  = UINT64_C(0xf1de83e19937733d);

static _Atomic uint64_t vprng_internal_inc_id = 1;

void     vprng_global_id_set(uint64_t id) { atomic_store(&vprng_internal_inc_id, id);   }
uint64_t vprng_global_id_get(void)        { return atomic_load(&vprng_internal_inc_id); }

//#warning "testing vprng_addtive_next hack in progress"

// returns an additive constant for the state update
static uint64_t vprng_additive_next(void)
{
  uint64_t b;

  do {
    // atomically increment the global counter and
    // convert it into a candidate additive constant
    b  = atomic_fetch_add_explicit(&vprng_internal_inc_id,
				   1,
				   memory_order_relaxed);
    b  = (b<<1)|1;
    b *= vprng_internal_inc_k;

    uint32_t pop = vprng_pop(b);
    uint32_t t   = pop - (32-8);

    // temp hack to get a feel if this is useful with weak finalizer
    // not clear ATM.
    //if ((b >> (64-2)) == 0) continue;

    if (t <= 2*8) {
      uint32_t str = vprng_pop(b & (b ^ (b >> 1)));
      if (str >= (pop >> 2)) return b;
    }
  } while(1);
}
#endif

//*******************************************************************

// initialize position in stream to zero
static inline void vprng_pos_init(vprng_t* prng)
{
  u64x4_t v = vprng_inc(prng);

  prng->state    = v >> 1;
  prng->state[0] = v[0];
}

#if !defined(VPRNG_ADDITIVE_CONSTANT_EXTERN)

// initializes the generator to the next set of additive constants.
void vprng_init(vprng_t* prng)
{
#if !defined(VPRNG_HIGHLANDER)  
  prng->inc[0]  = vprng_additive_next();
  prng->inc[1]  = vprng_additive_next();
  prng->inc[2]  = vprng_additive_next();
  prng->inc[3]  = vprng_additive_next();
#endif  

  vprng_pos_init(prng);
}

#else
extern void vprng_init(vprng_t* prng);
#endif

#ifndef VPRNG_CVPRNG_3TERM

// Brent's 2 term XorShift (default)
void cvprng_init(cvprng_t* prng)
{
  // inital state values. call x_i the xorshift sequence where:
  //   x_0 = 1 (define)
  //   x_i = xorshift(x_{i-1})
  // then the inital values are {p0,p1,p2,p3} where the p's
  // are approximately maximum distance apart (2^62). Start with
  //   off = RoundToOdd[Floor[2^64 Sqrt[2]]] = 0x6a09e667f3bcc908
  // just not to start at 0 and then each is further offset by
  // a smallish prime which is intended to lower the chances
  // that all four are in 'zeroland' at the same time. Zeroland
  // doesn't look to be a problem however (see doc)

  // {x_p0,x_p1,x_p2,x_p3}
  static const u64x4_t k = {UINT64_C(0x55a07167039ee1bb),  // p0 = 0*2^62 + off
			    UINT64_C(0x2078e461244b6d4b),  // p1 = 1*2^62 + off + 31
			    UINT64_C(0xfb187856a5f450ff),  // p2 = 2*2^62 + off + 257
			    UINT64_C(0x7f6efb9bf3fc45e1)}; // p3 = 2*2^62 + off + 541

  vprng_init(&(prng->base));
  prng->f2[0] = k;  
}

// only used for testing
// broken choices for statistical testing: {x_0,x_1,x_2,x_3}
// for higher confidence levels if passes tests
static const u64x4_t cvprng_hobble_init_k =
{
  UINT64_C(0x1),
  UINT64_C(0x81),
  UINT64_C(0x4021),
  UINT64_C(0x204089)
};

#else

// Optional three term XorShift: (I+L^b)(I+R^b)(I+L^a)
// This is from Francois Panneton's Thesis. Choosen
// from the ones with the top measures at uniformity.

void cvprng_init(cvprng_t* prng)
{
  // otherwise comments follow that of the 2 term

  static const u64x4_t k = {UINT64_C(0xc5da252a1302a152),
			    UINT64_C(0x7a888ce4d7ff17cc),
			    UINT64_C(0xf3e14609aa2f25ea),
			    UINT64_C(0x1af45fbbbc2fa67f)};

  
  vprng_init(&(prng->base));
  prng->f2[0] = k;  
}

// only used for testing
// broken choices for statistical testing: {x_0,x_1,x_2,x_3}
// for higher confidence levels if passes tests
static const u64x4_t cvprng_hobble_init_k =
{
  UINT64_C(0x1),
  UINT64_C(0x81200000409),
  UINT64_C(0x4800000024100049),
  UINT64_C(0xc1220c9344d90601)
};
#endif



// mod-inverse of 64-bit integer
static uint64_t vprng_modinv(uint64_t a)
{
  uint64_t x = (3*a)^2; 
  uint64_t y  = 1 - a*x;
  x = x*(1+y); y *= y;
  x = x*(1+y); y *= y;
  x = x*(1+y); y *= y;
  x = x*(1+y);

  return x;
}

#if !(defined(VPRNG_HIGHLANDER)||defined(VPRNG_ADDITIVE_CONSTANT_EXTERN))

uint64_t vprng_id_get(vprng_t* prng)
{
  // multiply by mod-inverse and nuke the "to odd" transform
  return (vprng_internal_inc_i*prng->inc[0])>>1;
}

uint64_t cvprng_id_get(cvprng_t* prng)
{
  return vprng_id_get(&prng->base);
}

// get the current position in the stream
uint64_t vprng_pos_get(vprng_t* prng)
{
  return prng->state[0] * vprng_modinv(prng->inc[0]) - 1;
}

#else

uint64_t vprng_id_get (vprng_unused vprng_t*   prng) { return 0; }
uint64_t cvprng_id_get(vprng_unused cvprng_t*  prng) { return 0; }

uint64_t vprng_pos_get(vprng_t* prng)
{
  // temp hack: no need to compute modinv
  return prng->state[0] * vprng_modinv(vprng_inc(prng)[0]) - 1;
}

#endif


// moves position in stream by 'off'
void vprng_pos_inc(vprng_t* prng, uint64_t off)
{
  prng->state += vprng_inc(prng) * off;
}

// set the stream to position 'pos'
void vprng_pos_set(vprng_t* prng, uint64_t pos)
{
  vprng_pos_init(prng);
  vprng_pos_inc(prng,pos);
}

#else
extern void     vprng_global_id_set(uint64_t id);
extern uint64_t vprng_global_id_get(void);

extern void     vprng_init (vprng_t* prng);
extern void     cvprng_init(cvprng_t* prng);

extern uint64_t vprng_id_get (vprng_t* prng);
extern uint64_t cvprng_id_get(cvprng_t* prng);

extern uint64_t vprng_pos_get(vprng_t* prng);
extern void     vprng_pos_set(vprng_t* prng, uint64_t pos);
extern void     vprng_pos_off(vprng_t* prng, uint64_t pos);

static inline uint64_t cvprng_pos_get(cvprng_t* prng) { return vprng_pos_get(&(prng->base)); }

#endif


//*******************************************************************
// Stripped down "portable" 256 bit SIMD via vector_size extension

// routines for strict aliasing happpy conversion.
static inline i32x8_t vprng_cast_i32(u32x8_t u) { i32x8_t r; memcpy(&r,&u,32); return r; }
static inline i64x4_t vprng_cast_i64(u64x4_t u) { i64x4_t r; memcpy(&r,&u,32); return r; }
static inline f32x8_t vprng_cast_f32(u32x8_t u) { f32x8_t r; memcpy(&r,&u,32); return r; }
static inline f64x4_t vprng_cast_f64(u64x4_t u) { f64x4_t r; memcpy(&r,&u,32); return r; }

// if enabled some explicit FMA usages are disabled and is reverted to the
// complier and command-line options discretion. produced results are
// identical in either case. default assumes FMAs are "fast".
#if !defined(VPRNG_DISBLE_OPTIONAL_FMA)

static inline f32x8_t vprng_splat_fmaf(float a, f32x8_t b, float c)
{
  f32x8_t r; for(int i=0; i<8; i++) r[i] = fmaf(a,b[i],c); return r;
}

static inline f64x4_t vprng_splat_fma(double a, f64x4_t b, double c)
{
  f64x4_t r; for(int i=0; i<4; i++) r[i] = fma(a,b[i],c); return r;
}

#else
static inline f32x8_t vprng_splat_fmaf(float a, f32x8_t b, float c)   { return a*b+c; }
static inline f64x4_t vprng_splat_fma(double a, f64x4_t b, double c)  { return a*b+c; }
#endif


//*******************************************************************
// need to jump through some hoops for integer to floating-point
// conversion. want a single op if possible. sorry. can't be helped ATM.
// both take integers with low bits of FP precision and convert
 
static inline f32x8_t vprng_f32x8_i(u32x8_t u)
{
#if   (VPRNG_CVT_F32_METHOD == 0)
  return 0x1.0p-24f * __builtin_convertvector(vprng_cast_i32(u), f32x8_t);
#elif (VPRNG_CVT_F32_METHOD == 1)
  return 0x1.0p-24f * __builtin_convertvector(u, f32x8_t);
#elif (VPRNG_CVT_F32_METHOD == 2)
  f32x8_t s = vprng_cast_f32(u | 0x3f800000);

  return vprng_splat_fmaf(0x1.0p-24f, s, -1.f);
#else
#error "VPRNG_CVT_F32_METHOD not selected"  
#endif  
}

static inline f64x4_t vprng_f64x4_i(u64x4_t u)
{
#if   (VPRNG_CVT_F64_METHOD == 0)
  return 0x1.0p-53 * __builtin_convertvector(vprng_cast_i64(u), f64x4_t);
#elif (VPRNG_CVT_F64_METHOD == 1)
  return 0x1.0p-53 * __builtin_convertvector(u, f64x4_t);
#elif (VPRNG_CVT_F64_METHOD == 2)
  f64x4_t d = vprng_cast_f64(u | UINT64_C(0x3ff0000000000000));

  return vprng_splat_fma(0x1.0p-53, d, -1.0);
#else
#error "VPRNG_CVT_F64_METHOD not selected"  
#endif  
}


//*******************************************************************

static inline u64x4_t vprng_u64x4 (vprng_t*  prng) { return vprng_cast_u64( vprng_u32x8(prng)); }
static inline u64x4_t cvprng_u64x4(cvprng_t* prng) { return vprng_cast_u64(cvprng_u32x8(prng)); }

static inline f64x4_t vprng_f64x4 (vprng_t*  prng) { return vprng_f64x4_i( vprng_u64x4(prng) >> 11); }
static inline f32x8_t vprng_f32x8 (vprng_t*  prng) { return vprng_f32x8_i( vprng_u32x8(prng) >>  8); }
static inline f64x4_t cvprng_f64x4(cvprng_t* prng) { return vprng_f64x4_i(cvprng_u64x4(prng) >> 11); }
static inline f32x8_t cvprng_f32x8(cvprng_t* prng) { return vprng_f32x8_i(cvprng_u32x8(prng) >>  8); }


// temp hack...nuke before making repo or clean-up into properly useful
static inline void vprng_block_fill_u32(uint32_t n, u32x8_t buffer[static n], vprng_t* prng)
{
  for(uint32_t i=0; i<n; i++) { buffer[i] = vprng_u32x8(prng); }
}

static inline void cvprng_block_fill_u32(uint32_t n, u32x8_t buffer[static n], cvprng_t* prng)
{
  for(uint32_t i=0; i<n; i++) { buffer[i] = cvprng_u32x8(prng); }
}


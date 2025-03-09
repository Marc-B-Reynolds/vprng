// Marc B. Reynolds, 2025
// Public Domain under http://unlicense.org, see link for details.

// example customization header for minimum targets w/supported
// SIMD 64-bit products. This make an off-the-shelf SplitMix64
// that's four wide. Difference vs. the implementation in the
// standard Java library (java.util.SplittableRandom) are:
// * since scalar Java discards bits for 32-bit results. It'd be
//   a loss to 'cache' and here it's for free. It also uses
//   a hybrid method for finalizing. Since it's dropping 32-bit
//   it uses a cheaper finalizer (MIX13)
// * different method of selecting the additive constant to
//   parameterize generators.
//
// Ideally this should be "more" different. I'd be much happier
// with four unique finalizers BUT the trick is finding a good
// set which all have the same shift amounts. Also that means
// (potentially..compiler+arch dep) more data loads...so maybe not?
// (could load 1 64-bit + splat per constant with all the same)
//
// By my measure 'mix03' is outperforming mix14 on low entropy
// inputs. 

#pragma once

#define VPRNG_NAME "vsplitmix"
#define VPRNG_MIX_EXTERNAL

#include "vprng.h"

// MIX14: http://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html
static inline u32x8_t vprng_mix(u64x4_t x)
{
  x ^= x >> 30; x *= UINT64_C(0x4be98134a5976fd3);
  x ^= x >> 29; x *= UINT64_C(0x3bc0993a5ad19a13);
  x ^= x >> 31;

  return vprng_cast_u32(x);
}




#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>

#if defined(__AVX2__)
#include <x86intrin.h>
#else
#endif

#define LENGTHOF(X) (sizeof(X)/sizeof(X[0]))

// inform headers to include any testing
#define VPRNG_SELF_TEST

#define HEADER     "\033[95m"
#define OKBLUE     "\033[94m"
#define OKCYAN     "\033[96m"
#define OKGREEN    "\033[92m"
#define WARNING    "\033[93m"
#define FAIL       "\033[91m"
#define ENDC       "\033[0m"
#define BOLD       "\033[1m"
#define UNDERLINE  "\033[4m"

void test_banner(char* str)
{
  printf(BOLD "TESTING: %s" ENDC "\n", str);
}

void test_name(char* str)
{
  printf("  %-22s ", str);
  fflush(stdout);
}

// note: pass returns zero and fail one.
uint32_t test_fail(void) { printf(FAIL "FAIL!" ENDC "\n");     return 1; }
uint32_t test_pass(void) { printf(OKGREEN "passed" ENDC "\n"); return 0; }


uint32_t test_u64_eq(uint64_t a, uint64_t b)
{
  if (a==b) return test_pass();
  return test_fail();
}



// build using default or whatever variant
#define VPRNG_IMPLEMENTATION
#ifndef VPRNG_INCLUDE
#include "vprng.h"
#else
#include VPRNG_INCLUDE
#endif

#include "common.h"

bool u64x4_eq(u64x4_t a, u64x4_t b)
{
  // vpxor   ymm0, ymm0, ymm1
  // vptest  ymm0, ymm0
  // sete    al
  return memcmp(&a, &b, sizeof(u64x4_t)) == 0;
}

void dump2_u64x4(u64x4_t a, u64x4_t b)
{
  printf("\n    "
	 "%16"  PRIx64
	 ":%16" PRIx64
	 ":%16" PRIx64
	 ":%16" PRIx64
	 " <- expected\n   " FAIL
	 " %16" PRIx64
	 ":%16" PRIx64
	 ":%16" PRIx64
	 ":%16" PRIx64
	 ENDC " <- got\n  "
	 ,
	 a[3],a[2],a[1],a[0],
	 b[3],b[2],b[1],b[0]
	 );
}

u32x8_t mod_inverse_u32x8(u32x8_t a)
{
  u32x8_t x = (3*a)^2; 
  u32x8_t y = 1 - a*x;
  x = x*(1+y); y *= y;
  x = x*(1+y); y *= y;
  x = x*(1+y);
  return x;
}

// with constant 'n' these expand to the minimal number of steps.
// (not here but if speed was important then these can be
//  reworked into precomputing a carryless mod-inverse of '2^n+1'
//  and application becomes a carryless product of that and 'x'
//  SEE: {cl,cr}_mod_inv_64 and {cl,cr}_mul_64 in
//  https://github.com/Marc-B-Reynolds/Stand-alone-junk/blob/master/src/SFH/carryless.h
static inline u64x4_t rxorshift_inv_64x4(u64x4_t x, uint32_t n)
{
  while(n < 64) { x ^= x >> n; n += n; } return x;
}
static inline u64x4_t lxorshift_inv_64x4(u64x4_t x, uint32_t n)
{
  while(n < 64) { x ^= x << n; n += n; } return x;
}



#if !defined(VPRNG_INCLUDE)

uint32_t check_basic(void)
{
  uint32_t errors = 0;
  
  test_banner("basic");

  test_name("base weyl");
  errors += test_u64_eq(vprng_internal_inc_k*vprng_internal_inc_i,1);

  return errors;
}

uint32_t check_inv(vprng_t* prng)
{
  test_name("mix internal sanity:");
  
  // currently can handle modification the multiplicative
  // constants. compute the inverses here.
  u32x8_t i0 = mod_inverse_u32x8(finalize_m0);
  u32x8_t i1 = mod_inverse_u32x8(finalize_m1);

  // would require tweaks to automatically handle any
  // changes of the xorshift constants.
  
  for(uint32_t i=0; i<33; i++) {
    u64x4_t u0 = prng->state;
    u64x4_t x  = vprng_u64x4(prng);
    
    // start inverse
    x = rxorshift_inv_64x4(x,17); // x ^= x >> 17 (inverse)
    x = vprng_mix_mul(x, i1);
    x = lxorshift_inv_64x4(x,15); // x ^= x << 15
    x = vprng_mix_mul(x, i0);
    x = rxorshift_inv_64x4(x,16); // x ^= x >> 16 (inverse)

    x = rxorshift_inv_64x4(x,33); // x ^= x >> 33 (inverse)
    
    if (u64x4_eq(u0,x)) continue;
    return test_fail();
  } 

  return test_pass();
}


// spot check position in stream manipulation
uint32_t check_pos(vprng_t* prng)
{
  vprng_t copy = *prng;
  
  test_name("pos_{get,set}:");
  
  uint64_t initial = vprng_pos_get(prng);
  
  for(uint32_t i=0; i<15; i++) {
    uint64_t pos  = vprng_pos_get(prng);
    
    vprng_pos_set(&copy, pos);
    
    bool same = u64x4_eq(prng->state, copy.state);
    
    vprng_u64x4(prng);

    if (same && (pos == initial + i)) continue;
    
    return test_fail();
  }

  return test_pass();
}
#endif


int main(void)
{
#if !defined(VPRNG_INCLUDE)
  vprng_t  prng;
  cvprng_t cprng;

  vprng_global_id_set(1);
  vprng_init(&prng);
  cvprng_init(&cprng);
  
  uint32_t errors = 0;

  errors += check_basic();
  errors += check_inv(&prng);
  errors += check_pos(&prng);


  if (!errors) {
    return 0;
  }
#else
  test_banner(VPRNG_NAME);
#if defined(SELF_TEST)
  return (int)self_test();
#else
  printf(OKGREEN "  has no specialized tests defined\n" ENDC);
  return 0;
#endif
#endif  

  return -1;
}

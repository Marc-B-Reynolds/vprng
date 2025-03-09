// major hack to create the initial state values for the
// XorShift portion

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <m4ri/m4ri_config.h>
#include <m4ri/m4ri.h>

#include "vprng.h"     // just for some defines
#include "common.h"
#include "xorshift.h"

// create initial state values for the XorShift
// let the first value of the "sequence" be one then

#ifndef VPRNG_CVPRNG_HOBBLE_INIT
#define OFF  (0x6a09e667f3bcc909ULL)
#define P0   (0x0000000000000000ULL+OFF)
#define P1   (0x4000000000000000ULL+OFF+31)
#define P2   (0x8000000000000000ULL+OFF+257)
#define P3   (0xc000000000000000ULL+OFF+541)
#else
#define P0   (0)
#define P1   (1)
#define P2   (2)
#define P3   (3)
#endif

#define XORSHIFT_A 10
#define XORSHIFT_B 7
#define XORSHIFT_C 33

#if 0
( 23, 17, 25)
( 23, 13, 38)
( 10, 7, 33)
( 11, 5, 32)
#endif

// copy/paste/hack code-----

mzd_t* m4ri_alloc_n(uint32_t n)
{
  return mzd_init((rci_t)n,(rci_t)n);
}

mzd_t* m4ri_alloc_64(void) { return m4ri_alloc_n(64); }

inline mzd_t* mat_mul(mzd_t* r, mzd_t* a, mzd_t* b) { return mzd_mul(r,a,b,0); }

// Does M4RI have this and I'm just not finding it?
void m4ri_pow(mzd_t* m, uint64_t n)
{
  mzd_t* s = mzd_copy(0,m);
  mzd_t* t = mzd_copy(0,m);
  
  mzd_set_ui(m,1);

  while(n != 0) {
    if (n & 1) {
      t = mzd_copy(t,m);
      m = mat_mul(m,t,s);
    }
    
    mat_mul(t,s,s);
    mzd_copy(s,t);
    n >>= 1;
  }
  
  mzd_free(t);
  mzd_free(s);
}

// convert dense matrix into preallocated M4RI
void to_m4ri(mzd_t* d, uint64_t s[static 64])
{
  const rci_t D = 64;
  
  for(rci_t r=0; r<D; r++) {
    uint64_t row = s[r];
    for(rci_t c=0; c<D; c++) {
      mzd_write_bit(d,r,c, row & 1);
      row >>= 1;
    }
  }
}

// column 'c' of 64x64 matrix
uint64_t get_col_m4ri(mzd_t* m, uint32_t c)
{
  uint64_t r = 0;

  for(uint32_t i=0; i<64; i++)
    r |= (uint64_t)mzd_read_bit(m,(rci_t)i,(rci_t)c) << i;

  return r;
}

// make a dense matrix from function 'f'
uint64_t func_to_mat(uint64_t m[static 64], uint64_t (*f)(uint64_t))
{
  uint64_t t = f(0);
  uint64_t p = 1;

  for(uint32_t i=0; i<64; i++) { m[i]=0; }

  do {
    uint64_t r = f(p) ^ t;
    uint64_t b = 1;

    for(int j=0; j<64; j++) {
      m[j]  ^= ((r & b) != 0) ? p : 0;
      b    <<= 1;
    }
    
    p <<= 1;
  } while(p);

  return t;
}


// the state update function

#if  0

#define NAME "brent"

uint64_t xorshift(uint64_t x)
{
  x ^= x << 7;
  x ^= x >> 9;

  return x;
}

#elif 1

#define NAME "x1("

// X1
uint64_t xorshift(uint64_t x)
{
  x ^= x << XORSHIFT_A;
  x ^= x >> XORSHIFT_B;
  x ^= x << XORSHIFT_C;
  
  return x;
}

#elif 2

#endif



const uint64_t offsets[] = {P0,P1,P2,P3};

xorshift_def_t* def;



uint64_t build_m(uint64_t x)
{
  return def->m(x,def->k[0],def->k[1],def->k[2]);
}

void build_3_term(void)
{
  uint64_t d[64];
  mzd_t*   m = m4ri_alloc_64();
  mzd_t*   p = m4ri_alloc_64();
  uint64_t r = 1;

  for(uint32_t i=0; i<LENGTHOF(xorshift_def); i++) {
    def = xorshift_def+i;

    // create the matrix (M)
    func_to_mat(d, build_m);
    to_m4ri(m,d);

    // check that it's invertiable which means it's full rank
    mzd_copy(p,m);
    int legal = (mzd_echelonize(p,1) == 64);

    if (!legal) printf("WAT!"); 
    

    // lazy hack the "name"
    {
      char* xn;
      
      if      (xorshift_def[i].m == xorshift_x1) xn = "X1";
      else if (xorshift_def[i].m == xorshift_x2) xn = "X2";
      else if (xorshift_def[i].m == xorshift_x5) xn = "X5";
      else                                       xn = "??";

      uint32_t* p = &(xorshift_def[i].k[0]);
      printf("%s(%u,%u,%u)\n", xn,p[0],p[1],p[2]);
    }

    // initial state values
    printf(" {");

    // compute M^p (p from array offsets)
    for(uint32_t i=0; i<4; i++) {
      mzd_copy(p,m);
      m4ri_pow(p,offsets[i]);

      // result is the first column since
      // we're transforming '1'.
      r = get_col_m4ri(p,0);
      printf("0x%016" PRIx64 ",", r);
    }
    
    printf("\b}\n");

    // hobbled initial state numbers
    printf(" {0x1");

    // compute M^i 
    mzd_copy(p,m);

    for(uint32_t i=1; i<5; i++) {
      // dumb. but so what.
      mzd_copy(p,m);
      m4ri_pow(p,i);
      r = get_col_m4ri(p,0);
      printf(",0x%" PRIx64, r);
    }
    
    printf("}\n");
    
    
    printf("\n");
  }
}


int main(void)
{
  build_3_term();
  
  return 0;
}

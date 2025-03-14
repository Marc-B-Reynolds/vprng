
// garbage bin of checking claims, etc.

#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <math.h>
#include <stdio.h>

#define VPRNG_IMPLEMENTATION
#include "vprng.h"

#include "common.h"
#include "xorshift.h"

static inline int64_t  asr_s64(int64_t  x, uint32_t n) { return x >> (n & 0x3f); }
static inline uint64_t asr_u64(uint64_t x, uint32_t n) { return (uint64_t)asr_s64((int64_t )x, n); }

static inline uint64_t pop_next_64(uint64_t x)
{
  uint64_t t = x + (x & -x);
  x = x & ~t;
  x = asr_u64(x, ctz_64(x));
  x = asr_u64(x, 1);
  return t^x;
}

// just some "bad" ones to peek at
uint64_t xorshift_b0(uint64_t x) { return xorshift_x1(x,1, 1,55); } // 
uint64_t xorshift_b1(uint64_t x) { return xorshift_x2(x,1, 1,55); } // 
uint64_t xorshift_b2(uint64_t x) { return xorshift_x5(x,1, 1,55); } //

uint64_t weyl_phi(uint64_t x)     { return x + UINT64_C(0x9e3779b97f4a7c15); }

typedef struct { char* name; uint64_t (*f)(uint64_t); } f_def_t;

f_def_t xorshift_table[] = {
#if 1
  // order as in the vprng.md.html
  {.name="weyl(1/phi) ", .f=weyl_phi   },
  {.name="x5(10, 7,33)", .f=xorshift_1 },
  {.name="x2(15,17,27)", .f=xorshift_4 },
  {.name="x1(23,15,25)", .f=xorshift_8 },
  {.name="x2( 5,15,27)", .f=xorshift_0 },
  {.name="x1(10, 7,33)", .f=xorshift_2 },
  {.name="x1(11, 5,32)", .f=xorshift_9 },
  {.name="x5( 6,23,27)", .f=xorshift_3 },
  {.name="x2(19,43,27)", .f=xorshift_5 },
  {.name="2-term      ", .f=xorshift2 },
  {.name="x2(21, 9,29)", .f=xorshift_7 },
  {.name="x2(11, 5,32)", .f=xorshift_a },
  {.name="x1(23,13,38)", .f=xorshift_6 },
#else  
  {.name="x1(11, 5,32)", .f=xorshift_b0 },
  {.name="x2(11, 5,32)", .f=xorshift_b1 },
  {.name="x1(11, 5,32)", .f=xorshift_b2 },
#endif
};

uint64_t xorshift3(uint64_t s)
{
  s ^= s << 10;
  s ^= s >>  7;
  s ^= s << 33;
  return s;
}

// Generate zeroland information per XorShift transform:
// * escape time = number of steps for input u_0 to reach at least 16 bits set
// * for all u_0 of a given popcount sum the number of steps to reach, track
//   the maximum and produce mean and stddev.

//#define ZL_POP_CHECK 8
#define ZL_POP_CHECK 5      // just for faster

void zeroland_header(void)
{
  printf("| function     |");

  for(uint32_t c=0; c<ZL_POP_CHECK; c++)
    printf("%11s %u|","pop", c+1);

  printf("\n");

  printf("|  ---         |");

  for(uint32_t c=0; c<ZL_POP_CHECK; c++)
    printf("%13s|","---");
  
  printf("\n");
}

void zeroland_inspect(char* name, uint64_t (*f)(uint64_t))
{
  uint32_t max = 0;

  printf("|`%s`|", name);

  for(uint32_t p0=1; p0 <= ZL_POP_CHECK; p0++) {
    seq_stats_t stats;
    seq_stats_init(&stats);
    
    uint64_t u0 = (UINT64_C(1)<<p0)-1;
    uint64_t u1;
    uint32_t p1;
    
    do {
      uint32_t et = 1;
      
      u1 = f(u0);
      p1 = pop_64(u1);
      
      // find the "escape time" of u1.
      while(p1 < 16) {
	u1 = f(u1);
	p1 = pop_64(u1);
	et++;
      }
      
      seq_stats_add(&stats, (double)et);
      
      max = max >= et ? max : et;
      
      u0 = pop_next_64(u0);
    } while(u0 != UINT64_C(-1));
    
    printf("%5.3f ± %5.3f|",
	   seq_stats_mean(&stats),
	   seq_stats_stddev(&stats));
  }
  printf("%2u|\n", max);
}


void zeroland_info(void)
{
  zeroland_header();
  
  for(uint32_t id=0; id<LENGTHOF(xorshift_table); id++)
    zeroland_inspect(xorshift_table[id].name, xorshift_table[id].f);
}



// count the number of 32-bit accepted candidates (vpcg32)
void vpcg32_count(void)
{
  printf("vpcg32_count\n");

  static const uint32_t vpcg_internal_inc_k  = UINT32_C(0x9e3779b9);
  static const uint32_t vpcg_internal_inc_i  = UINT32_C(0x144cbc89);
  static const uint32_t vpcg_internal_inc_t  = vpcg_internal_inc_k*vpcg_internal_inc_i;

  uint32_t counter  = 0;
  uint32_t accepted = 0;
  uint32_t b;

  if (vpcg_internal_inc_t != 1) printf("ERROR!\n");
  
  // obviously any modifications need to be manually updated here
  do {
    b  = counter++;
    b  = (b<<1)|1;
    b *= vpcg_internal_inc_k;
    
    uint32_t pop = pop_32(b);
    uint32_t t   = pop - (16-4);

    // top 2 bit rejection (don't think this is interesting)
    //if ((b >> (32-2))==0) continue;
    
    if (t <= 2*8) {
      uint32_t str = pop_32(b & (b ^ (b >> 1)));
      if (str >= (pop >> 2)) accepted++;
    }
  } while(counter < 0x80000000);

  printf("vpcg32: %u (%08x) accepted additive constants\n", accepted,accepted);
  
}

int main(void)
{
  // just hack whatever thing I'm wanted to validate

  {
    uint64_t u = 1;
    uint64_t c = 0;
    uint64_t s = 0;
    uint64_t min = 0xfffffffffff;
    
      seq_stats_t stats;
      seq_stats_init(&stats);
    do {
      u = weyl_phi(u);
      c++;
      if (pop_64(u) > 15) continue;

      if (c < min) min = c;
      seq_stats_add(&stats, (double)c);
      //printf("%lu ", c);
      c = 0;
      s++;
      
    } while(s < 0xffff);

    printf("\n\n");
        printf("%5.3f ± %5.3f| min = %lu",
	   seq_stats_mean(&stats),
	       seq_stats_stddev(&stats),
	       min);

	return 0;
  }


  
  //vpcg32_count();  return 0;
  zeroland_info(); return 0;

  vprng_t prng;

  vprng_global_id_set(1);
  
  for(uint32_t i=0; i<10; i++) {
    vprng_init(&prng);
    printf("%2llu,", (unsigned long long)vprng_id_get(&prng));
  }
  printf("\n");

  vprng_global_id_set(1);

  for(uint32_t i=0; i<10; i++) {
    vprng_init(&prng);
    printf("%2llu,", (unsigned long long)vprng_global_id_get());
  }
  printf("\n");


  vprng_global_id_set(1);

  for(uint32_t i=0; i<10; i++) {
    vprng_init(&prng);
    printf("%016llx %016llx %016llx %016llx\n", (unsigned long long)prng.inc[0],(unsigned long long)prng.inc[1],(unsigned long long)prng.inc[2],(unsigned long long)prng.inc[3]);
  }
  // printf("");

  vprng_global_id_set(1);

  for(uint32_t i=0; i<10; i++) {
    vprng_init(&prng);
    printf("%2llu %2llu %2llu %2llu\n",
	   (unsigned long long)(vprng_internal_inc_i*prng.inc[0])>>1,
	   (unsigned long long)(vprng_internal_inc_i*prng.inc[1])>>1,
	   (unsigned long long)(vprng_internal_inc_i*prng.inc[2])>>1,
	   (unsigned long long)(vprng_internal_inc_i*prng.inc[3])>>1);
  }
  // printf("");
  
  return 0;
}

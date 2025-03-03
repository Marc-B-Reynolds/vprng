
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <math.h>
#include <stdio.h>

#define VPRNG_IMPLEMENTATION
#include "vprng.h"
#include "common.h"

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

// 2-term
uint64_t xorshift2(uint64_t s) { s ^= s << 7; s ^= s >> 9; return s; }

/*
|   a|   b|  c |$X_1$|$X_2$|$X_5$|
|---:|---:|---:| ---:| ---:| ---:|
|   5|  15|  27|  7  |**2**|  4  |
|  10|   7|  33|**3**|  4  |**2**|
|   6|  23|  27| 10  |  6  |**3**|
|  15|  17|  27|  6  |**3**|  6  |
|  19|  43|  27|  7  |**3**| 10  |
|  23|  13|  38|**3**|  4  |  4  |
|  21|   9|  29|  4  |**3**|  4  |
|  23|  17|  25|**3**|  4  | 10  |
|  11|   5|  32|**3**|**3**|  7  |

*/

uint64_t xorshift_x1(uint64_t x, uint32_t a, uint32_t b, uint32_t c)
{
  x ^= x << a;
  x ^= x >> b;
  x ^= x << c;
  return x;
}

uint64_t xorshift_x2(uint64_t x, uint32_t a, uint32_t b, uint32_t c)
{
  x ^= x << c;
  x ^= x >> b;
  x ^= x << a;
  return x;
}

uint64_t xorshift_x5(uint64_t x, uint32_t a, uint32_t b, uint32_t c)
{
  x ^= x << b;
  x ^= x << c;
  x ^= x >> a;
  return x;
}

uint64_t xorshift_0(uint64_t x) { return xorshift_x2(x, 5,15,27); } // 2
uint64_t xorshift_1(uint64_t x) { return xorshift_x5(x,10, 7,33); } // 2
uint64_t xorshift_2(uint64_t x) { return xorshift_x1(x,10, 7,33); } // 3
uint64_t xorshift_3(uint64_t x) { return xorshift_x5(x, 6,23,27); } // 3
uint64_t xorshift_4(uint64_t x) { return xorshift_x2(x,15,17,27); } // 3
uint64_t xorshift_5(uint64_t x) { return xorshift_x2(x,19,43,27); } // 3
uint64_t xorshift_6(uint64_t x) { return xorshift_x1(x,23,13,38); } // 3
uint64_t xorshift_7(uint64_t x) { return xorshift_x2(x,21, 9,29); } // 3
uint64_t xorshift_8(uint64_t x) { return xorshift_x1(x,23,15,25); } // 3
uint64_t xorshift_9(uint64_t x) { return xorshift_x1(x,11, 5,32); } // 3
uint64_t xorshift_a(uint64_t x) { return xorshift_x2(x,11, 5,32); } // 3


uint64_t xorshift_b0(uint64_t x) { return xorshift_x1(x,1, 1,55); } // 
uint64_t xorshift_b1(uint64_t x) { return xorshift_x2(x,1, 1,55); } // 
uint64_t xorshift_b2(uint64_t x) { return xorshift_x5(x,1, 1,55); } // 

typedef struct { char* name; uint64_t (*f)(uint64_t); } f_def_t;

f_def_t xorshift_table[] = {
#if 1
  {.name="2-term      ", .f=xorshift2 },
  {.name="x2( 5,15,27)", .f=xorshift_0 },
  {.name="x5(10, 7,33)", .f=xorshift_1 },
  {.name="x1(10, 7,33)", .f=xorshift_2 },
  {.name="x5( 6,23,27)", .f=xorshift_3 },
  {.name="x2(15,17,27)", .f=xorshift_4 },
  {.name="x2(19,43,27)", .f=xorshift_5 },
  {.name="x1(23,13,38)", .f=xorshift_6 },
  {.name="x2(21, 9,29)", .f=xorshift_7 },
  {.name="x1(23,15,25)", .f=xorshift_8 },
  {.name="x1(11, 5,32)", .f=xorshift_9 },
  {.name="x2(11, 5,32)", .f=xorshift_a },
#elif 1
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

#define ZL_POP_CHECK 8

void zeroland_info(void)
{
  printf("| function     |");

  for(uint32_t c=0; c<ZL_POP_CHECK; c++)
    printf("%11s %u|","pop", c+1);

  printf("\n");

  printf("|  ---         |");

  for(uint32_t c=0; c<ZL_POP_CHECK; c++)
    printf("%13s|","---");
  
  printf("\n");

  for(uint32_t id=0; id<LENGTHOF(xorshift_table); id++) {
    uint64_t (*f)(uint64_t) = xorshift_table[id].f;
    uint32_t max = 0;
    
    printf("|`%s`|", xorshift_table[id].name);

    for(uint32_t p0=1; p0 <= ZL_POP_CHECK; p0++) {
      seq_stats_t stats;
      seq_stats_init(&stats);

      uint64_t u0 = (1ull<<p0)-1;
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
}


int main(void)
{
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

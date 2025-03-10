#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wchar.h>
#include <string.h>
#include <math.h>

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

#include "vprng.h"
#include "common.h"


uint64_t get_timestamp(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void printb(uint64_t v, uint32_t b)
{
  uint64_t m = UINT64_C(1)<<(b-1);
  do { printf("%c", (v & m)!=0 ? '1':'.'); m >>= 1; } while (m != 0);
}

const uint32_t lcg_mul_k_table_32[] =
{
  0x915f77f5, // 1..1...1.1.11111.111.1111111.1.1 : 21  9
  0x93d765dd, // 1..1..1111.1.111.11..1.111.111.1 : 20 10
  0xadb4a92d, // 1.1.11.11.11.1..1.1.1..1..1.11.1 : 17 13
  0xa13fc965, // 1.1....1..11111111..1..1.11..1.1 : 17  9
  0x8664f205, // 1....11..11..1..1111..1......1.1 : 13  8
  0xcf019d85, // 11..1111.......11..111.11....1.1 : 15  7
  0xae3cc725, // 1.1.111...1111..11...111..1..1.1 : 17  9
  0x9fe72885, // 1..11111111..111..1.1...1....1.1 : 17  8
  0xae36bfb5, // 1.1.111...11.11.1.1111111.11.1.1 : 21 10
  0x82c1fcad, // 1.....1.11.....1111111..1.1.11.1 : 16  8
  0xac564b05, // 1.1.11...1.1.11..1..1.11.....1.1 : 14 11
  0x01c8e815, // .......111..1...111.1......1.1.1 : 11  7
  0x01ed0675, // .......1111.11.1.....11..111.1.1 : 14  7
  0x2c2c57ed, // ..1.11....1.11...1.1.111111.11.1 : 17  9
  0x5f356495, // .1.11111..11.1.1.11..1..1..1.1.1 : 17 11
  0x2c9277b5  // ..1.11..1..1..1..111.1111.11.1.1 : 17 10
};

void examine_current(u32x8_t f)
{
  for(uint32_t i=0; i<sizeof(lcg_mul_k_table_32)/4; i++) {
    uint32_t v = lcg_mul_k_table_32[i];

    printf("%08x ", v);
    printb(v,32);
    printf(" : %2u %2i\n", pop_32(v), bit_run_count_32(v));
  }
}


typedef struct
{
  uint32_t s0,m0,s1,m1,s2;
} sxm32_def_t;

sxm32_def_t sxm_def_table[] = {
#if 0  
  {15, 0x3a7ba96b, 13, 0x5e919299, 16}, // 0.19563436462680908
  {15, 0x49c34cd3, 13, 0xe7418ca7, 16}, // 0.18400092964673831
  {15, 0xf15f5959, 14, 0x7db29359, 16}, // 0.18103205436627479
  {15, 0xc62f4d53, 14, 0x62b8a46b, 16}, // 0.19973996656987172

  {15, 0xd168aaad, 15, 0xaf723597, 15}, // 0.15983776156606694
  {15, 0x1216ccb5, 15, 0x3abcdca9, 15}, // 0.19426091938816648
  {15, 0x81aab34d, 15, 0x18e746a3, 15}, // 0.19938572052445763
  {15, 0x4811acab, 15, 0x5591acd7, 16}, // 0.18522661033580071
  {15, 0x2548acd5, 15, 0x0b39d397, 16}, // 0.19121161052714156
  {15, 0x7f19c559, 15, 0xb356358d, 16}, // 0.19198007174447981
  
  {16, 0x88c0a94b, 14, 0x9d06da59, 17}, // 0.16898511658356749
  {16, 0xbd10754b, 14, 0x35a29b0d, 16}, // 0.19885203058591913
  {16, 0x83747333, 14, 0xaa256573, 16}, // 0.18105722344231542
  {16, 0xbe8b6ca7, 14, 0x6dd624b5, 16}, // 0.18223928664971270
  {16, 0xb921a6cb, 14, 0x30b5a6d1, 16}, // 0.19745192295417058
#endif  

  {16, 0x21f0aaad, 15, 0xf35a2d97, 15}, // 0.10734781817103507
  {16, 0x603a32a7, 15, 0x5a522677, 15}, // 0.17514135907753242
  {16, 0x21f0aaad, 15, 0xd35a2d97, 15}, // 0.10760229515479501
  {16, 0x97219aad, 15, 0xab46b735, 15}, // 0.19536391240344408
  {16, 0xb237694b, 15, 0xeb5b4593, 15}, // 0.17274184020173433
  {16, 0x8ee0d535, 15, 0x5dc6b5af, 15}, // 0.18664478683752250
  {16, 0xdc63b4d3, 15, 0x2c32b9a9, 15}, // 0.17368589564800074
  {16, 0x93f2552b, 15, 0x959b4a4d, 15}, // 0.18360629205797341

  {16, 0xdc85aaa7, 15, 0x6658a5cb, 15}, // 0.18577280285788791
  {16, 0x17cdd657, 15, 0xa426cb25, 15}, // 0.18995262675473334
  {16, 0xab39aacb, 15, 0xa1b5d19b, 15}, // 0.19045785238099658
  {16, 0xaecc96b5, 15, 0xf64dcd47, 15}, // 0.19077817816874504
  {16, 0x5dce3553, 15, 0xa655d8e9, 15}, // 0.19621753012889542
  {16, 0x604baa5d, 15, 0x43d6ce97, 15}, // 0.16491052655811722

#if 0  
  {16, 0xc3d9a965, 16, 0x362e4b47, 15}, // 0.19575424692659107
#endif

#if 0  
  {16, 0xa52fb2cd, 15, 0x551e4d49, 16}, // 0.17162579707098322
  {16, 0xac10d4eb, 15, 0x9d51b169, 16}, // 0.17676510450127819
  {16, 0x4bdc9aa5, 15, 0x2729b469, 16}, // 0.17355424787865850
  {16, 0xdf892d4b, 15, 0x3c2da6b3, 16}, // 0.18368195486921446
  {16, 0x7feb352d, 15, 0x846ca68b, 16}, // 0.17353355999581582
  {16, 0x462daaad, 15, 0x0a36c95d, 16}, // 0.18674876992866513
  {16, 0x4ffcab35, 15, 0xe98db28b, 16}, // 0.19423994132339928

  {16, 0xa812d533, 15, 0xb278e4ad, 17}, // 0.16540778981744320
  {16, 0x9c8f2d35, 15, 0x5d1346b5, 17}, // 0.16835348823718840
  {16, 0xe02bd533, 15, 0x0364c8ad, 17}, // 0.17447893149410759
  {16, 0x1ec9b4db, 15, 0x3224d38d, 17}, // 0.18631684392389897
  {16, 0xc845a997, 15, 0xf214db9b, 17}, // 0.19553179377831409
  {16, 0x0ae84d3b, 15, 0x3b9d4e5b, 17}, // 0.19776257374279985
  {16, 0x6872cd2d, 15, 0xf4a0d975, 17}, // 0.19992260539370590
#endif  

#if 0  
  {16, 0x418fb5b3, 15, 0x8cf3539b, 16}, // 0.19817117175199098
  {16, 0xf0ae2ad7, 15, 0x8965d939, 16}, // 0.19881758420284917
  {16, 0x3c9aa9ab, 16, 0x051369d7, 16}, // 0.19687211117412906
  {16, 0x054335ab, 15, 0x146da68b, 16}, // 0.19943843016872725
  
  {17, 0x7186cd35, 15, 0xfe6bba73, 15}, // 0.18312741727971640
  {17, 0xcd8512ad, 15, 0xb95c5a73, 15}, // 0.19050717016846502
  {17, 0x179cd515, 15, 0x4c495d47, 15}, // 0.19608530402798924
  {17, 0x0364d657, 15, 0xac2a34c5, 15}, // 0.19665754791333651
  {17, 0x78d31553, 15, 0xc547ac65, 15}, // 0.19918133404528665
  {17, 0x0ee6d967, 15, 0x9c8a4a33, 16}, // 0.19722490309575344  
  {17, 0x24f4d2cd, 15, 0x1ba3b969, 16}, // 0.19789489706453650
  {17, 0x9e485565, 16, 0xef1d6b47, 16}, // 0.16143129787074881
  {17, 0x88a5ad35, 16, 0x96338b27, 16}, // 0.19653922266398804
  {17, 0x9bde596b, 16, 0x1c9e9647, 16}, // 0.19882570872036193
  {17, 0xa1c76a55, 16, 0x5ca46b97, 16}, // 0.19959562213253398
  {18, 0xa136aaad, 16, 0x9f6d62d7, 17}, // 0.19768193144773874
#endif  
};

sxm32_def_t sxm_def_og[] = {
  {16, 0x21f0aaad, 15, 0xf35a2d97, 15}, // 0.10734781817103507
  {16, 0x21f0aaad, 15, 0xd35a2d97, 15}, // 0.10760229515479501
  {15, 0xd168aaad, 15, 0xaf723597, 15}, // 0.15983776156606694
  {17, 0x9e485565, 16, 0xef1d6b47, 16}, // 0.16143129787074881
  {16, 0x604baa5d, 15, 0x43d6ce97, 15}, // 0.16491052655811722
  {16, 0xa812d533, 15, 0xb278e4ad, 17}, // 0.16540778981744320
  {16, 0x9c8f2d35, 15, 0x5d1346b5, 17}, // 0.16835348823718840
  {16, 0x88c0a94b, 14, 0x9d06da59, 17}, // 0.16898511658356749
  {16, 0xa52fb2cd, 15, 0x551e4d49, 16}, // 0.17162579707098322
  {16, 0xb237694b, 15, 0xeb5b4593, 15}, // 0.17274184020173433
  {16, 0x7feb352d, 15, 0x846ca68b, 16}, // 0.17353355999581582
  {16, 0x4bdc9aa5, 15, 0x2729b469, 16}, // 0.17355424787865850
  {16, 0xdc63b4d3, 15, 0x2c32b9a9, 15}, // 0.17368589564800074
  {16, 0xe02bd533, 15, 0x0364c8ad, 17}, // 0.17447893149410759
  {16, 0x603a32a7, 15, 0x5a522677, 15}, // 0.17514135907753242
  {16, 0xac10d4eb, 15, 0x9d51b169, 16}, // 0.17676510450127819
  {15, 0xf15f5959, 14, 0x7db29359, 16}, // 0.18103205436627479
  {16, 0x83747333, 14, 0xaa256573, 16}, // 0.18105722344231542
  {16, 0xbe8b6ca7, 14, 0x6dd624b5, 16}, // 0.18223928664971270
  {17, 0x7186cd35, 15, 0xfe6bba73, 15}, // 0.18312741727971640
  {16, 0x93f2552b, 15, 0x959b4a4d, 15}, // 0.18360629205797341
  {16, 0xdf892d4b, 15, 0x3c2da6b3, 16}, // 0.18368195486921446
  {15, 0x49c34cd3, 13, 0xe7418ca7, 16}, // 0.18400092964673831
  {15, 0x4811acab, 15, 0x5591acd7, 16}, // 0.18522661033580071
  {16, 0xdc85aaa7, 15, 0x6658a5cb, 15}, // 0.18577280285788791
  {16, 0x1ec9b4db, 15, 0x3224d38d, 17}, // 0.18631684392389897
  {16, 0x8ee0d535, 15, 0x5dc6b5af, 15}, // 0.18664478683752250
  {16, 0x462daaad, 15, 0x0a36c95d, 16}, // 0.18674876992866513
  {16, 0x17cdd657, 15, 0xa426cb25, 15}, // 0.18995262675473334
  {16, 0xab39aacb, 15, 0xa1b5d19b, 15}, // 0.19045785238099658
  {17, 0xcd8512ad, 15, 0xb95c5a73, 15}, // 0.19050717016846502
  {16, 0xaecc96b5, 15, 0xf64dcd47, 15}, // 0.19077817816874504
  {15, 0x2548acd5, 15, 0x0b39d397, 16}, // 0.19121161052714156
  {15, 0x7f19c559, 15, 0xb356358d, 16}, // 0.19198007174447981
  {16, 0x4ffcab35, 15, 0xe98db28b, 16}, // 0.19423994132339928
  {15, 0x1216ccb5, 15, 0x3abcdca9, 15}, // 0.19426091938816648
  {16, 0x97219aad, 15, 0xab46b735, 15}, // 0.19536391240344408
  {16, 0xc845a997, 15, 0xf214db9b, 17}, // 0.19553179377831409
  {15, 0x3a7ba96b, 13, 0x5e919299, 16}, // 0.19563436462680908
  {16, 0xc3d9a965, 16, 0x362e4b47, 15}, // 0.19575424692659107
  {17, 0x179cd515, 15, 0x4c495d47, 15}, // 0.19608530402798924
  {16, 0x5dce3553, 15, 0xa655d8e9, 15}, // 0.19621753012889542
  {17, 0x88a5ad35, 16, 0x96338b27, 16}, // 0.19653922266398804
  {17, 0x0364d657, 15, 0xac2a34c5, 15}, // 0.19665754791333651
  {16, 0x3c9aa9ab, 16, 0x051369d7, 16}, // 0.19687211117412906
  {17, 0x0ee6d967, 15, 0x9c8a4a33, 16}, // 0.19722490309575344
  {16, 0xb921a6cb, 14, 0x30b5a6d1, 16}, // 0.19745192295417058
  {18, 0xa136aaad, 16, 0x9f6d62d7, 17}, // 0.19768193144773874
  {16, 0x0ae84d3b, 15, 0x3b9d4e5b, 17}, // 0.19776257374279985
  {17, 0x24f4d2cd, 15, 0x1ba3b969, 16}, // 0.19789489706453650
  {16, 0x418fb5b3, 15, 0x8cf3539b, 16}, // 0.19817117175199098
  {16, 0xf0ae2ad7, 15, 0x8965d939, 16}, // 0.19881758420284917
  {17, 0x9bde596b, 16, 0x1c9e9647, 16}, // 0.19882570872036193
  {16, 0xbd10754b, 14, 0x35a29b0d, 16}, // 0.19885203058591913
  {17, 0x78d31553, 15, 0xc547ac65, 15}, // 0.19918133404528665
  {15, 0x81aab34d, 15, 0x18e746a3, 15}, // 0.19938572052445763
  {16, 0x054335ab, 15, 0x146da68b, 16}, // 0.19943843016872725
  {17, 0xa1c76a55, 16, 0x5ca46b97, 16}, // 0.19959562213253398
  {15, 0xc62f4d53, 14, 0x62b8a46b, 16}, // 0.19973996656987172
  {16, 0x6872cd2d, 15, 0xf4a0d975, 17}, // 0.19992260539370590
};

static inline uint32_t hash(uint32_t x, const sxm32_def_t* const def)
{
  uint32_t m0 = def->m0;
  uint32_t m1 = def->m1;
  uint32_t s0 = def->s0;
  uint32_t s1 = def->s1;
  uint32_t s2 = def->s2;

  x = (x ^ (x >> s0)) * m0;
  x = (x ^ (x >> s1)) * m1;
  x = (x ^ (x >> s2));

  return x;
}


// goodness of fit statstics
typedef struct {
  double perBitX2;
  double iBitX2;
  double oBitX2;
  double maxBias;
  double iMaxBias;
  double oMaxBias;
} gof_t;

// oi = 1st index: bit position of flipped input
//      2nd index: bit position in output
typedef struct {
  uint32_t oi[32][32];  // per bit position counts
  uint32_t pop[33];     // population count
  uint32_t oiI[32];     // output bit
  uint32_t oiO[32];     // input bit
  gof_t    le;          // gof for low entropy input
//gof_t    he;          // gof for high entropy input
  uint32_t n;           // number of inputs processed
} stats32_t;

// Core of computing a chi-squared statistic where the expected
// distribution is uniform:  Sum[(oi[n]-e)^2]
//   oi: observed counts
//   e:  expected counts (scaled probablity)
//   n:  length of array (oi)
// The final division (and any extra required scaling) is up
// to the caller.
 double gof_chi_squared_eq(uint32_t* oi, uint32_t n, double e)
{
#if 0
  double r = 0.0;

  for (uint32_t i=0; i<n; i++) {
    double d = ((double)oi[i])-e;
    r = fma(d,d,r);
  }

  return r;
#else
  double r0 = 0.0;
  double r1 = 0.0;
  double r2 = 0.0;
  double r3 = 0.0;

  uint32_t j = n >> 2;
  uint32_t i = 0;

  while(j != 0) {
    double d0 = ((double)oi[i  ])-e;
    double d1 = ((double)oi[i+1])-e;
    double d2 = ((double)oi[i+2])-e;
    double d3 = ((double)oi[i+3])-e;
    r0 = fma(d0,d0,r0);
    r1 = fma(d1,d1,r1);
    r2 = fma(d2,d2,r2);
    r3 = fma(d3,d3,r3);
    i += 4; j--;
  }

  double re = 0.0;

  n &= 3;
  
  while (n != 0) {
    double d = ((double)oi[i])-e;
    re = fma(d,d,re);
    i++; n--;
  }
  return r0+r1+r2+r3+re;
#endif  
}

// walk the per-bit data and compute a chi-squared statistic
 double bias_chi_squared(stats32_t* s)
{
  uint32_t n = s->n;
  
  // each bit as p=.5 of being flipped so expected counts per bin is n/2
  double ei   = (double)n*0.5;
  double base = sqrt(gof_chi_squared_eq(&s->oi[0][0], 32*32, ei));

  // the 'n' input samples have each bit flipped (the extra 32 factor)
  return (100.0/(32.0*ei))*base;
}

void sac_gather_oio_32(stats32_t* s)
{
  for (uint32_t j=0; j<32; j++) {
    uint32_t sum = 0;
    for (uint32_t i=0; i<32; i++) {
      sum += s->oi[i][j];
    }
    s->oiO[j] = sum;
  }
}

// the worker function for SAC measurements:
// Given hash 'f' and sample input 'x' compute h = f(x)
// and then gather data for all bit flips of 'x'
static inline void sac_core_32(stats32_t* s, sxm32_def_t* def, uint32_t x)
{
  // compute the hash for input x (from sampling method)
  uint32_t h = hash(x,def);

  // flip each bit of input 'x'
  for (uint32_t i=0; i<32; i++) {
    uint32_t t   = h ^ hash(x ^ (1<<i),def);
    uint32_t pop = pop_32(t);

    // gather bit-by-bit counts
    for (uint32_t j=0; j<32; j++) {
      uint32_t b = (t >> j) & 1;
      s->oi[i][j] += b;
    }

    // gather population count data
    s->oiI[i] += pop;
    s->pop[pop]++;
  }
}

double bias_max(uint32_t* d, double s, uint32_t n)
{
  double max = 0.0;

  for(uint32_t i=0; i<n; i++) {
    double t = fabs(fma((double)d[i],s, -1.0));
    max = (max >= t) ? max : t;
  }
  
  return max;
}

void bias_foo(seq_stats_t* stats, uint32_t* d, double s, uint32_t n)
{
  for(uint32_t i=0; i<n; i++) {
    seq_stats_add(stats, 100.0*fabs(fma((double)d[i],s, -1.0)));
  }
}



// helper: chi-squared for oiI & oiO
double bias_chi_squared_rc(stats32_t* s, uint32_t* o)
{
  double e = (32.0/2.0)*(double)s->n;
  double r = sqrt(gof_chi_squared_eq(o, 32, e));
  return (100.0/e)*r;
}

void sac_gof_32(stats32_t* s, gof_t* gof)
{
  // 1/expected. expected = n/2
  double ie = 2.0/s->n;
  
  sac_gather_oio_32(s);
  gof->perBitX2 = bias_chi_squared(s);
  gof->iBitX2   = bias_chi_squared_rc(s,&s->oiI[0]);
  gof->oBitX2   = bias_chi_squared_rc(s,&s->oiO[0]);
  gof->maxBias  = 100.0*bias_max(&s->oi[0][0], ie, 32*32);
  gof->iMaxBias = 100.0*bias_max(&s->oiI[0],   ie*(1.0/32.0), 32);
  gof->oMaxBias = 100.0*bias_max(&s->oiO[0],   ie*(1.0/32.0), 32);

#if defined(DUMP_FIGURE)  
  bias_array(&s->oi[0][0], ie, 32,32);
#endif  
}

// globals because it'a all one big copy-pasta hackfest
uint32_t weyl_state = 0;
uint32_t weyl_inc   = 0x01010101;     // default really crappy choice


// gather counts with weyl sequence
void sac_vle_count_32(stats32_t* s, sxm32_def_t* def, uint32_t n)
{
  uint32_t x  = weyl_state;
  uint32_t dx = weyl_inc;
  
  s->n += n;
  
  for(uint32_t i=0; i<n; i++) {
    sac_core_32(s,def,x);
    x += dx;
  }
}

// helpers: gather 'n' samples and gen all statistics
void sac_vle_32(stats32_t* s, sxm32_def_t* def, uint32_t n)
{
  sac_vle_count_32(s,def,n);
  sac_gof_32(s, &s->le);
}


void bias_clear_counts(stats32_t* s)
{
  s->n = 0;
  
  memset(&s->oi,  0, sizeof(s->oi));
  memset(&s->oiI, 0, sizeof(s->oiI));
  memset(&s->oiO, 0, sizeof(s->oiO));
  memset(&s->pop, 0, sizeof(s->pop));
}

void gof_print(gof_t* gof)
{
  printf("max: %.14f, chi: %.14f : \n",
	 gof->maxBias,gof->perBitX2);
}


void test_table(void)
{
  printf("test table\n");

  uint32_t SAC_LEN = 0x00200000;

  SAC_LEN >>= 5;
  
  for(uint32_t i=0; i<LENGTHOF(sxm_def_table); i++) {
    seq_stats_t welford;
    
    sxm32_def_t* def =sxm_def_table+i;
    stats32_t stats = {.oi={{0}}};
    sac_vle_32(&stats,def, SAC_LEN);

    printf("%2u: ", i); //gof_print(&stats.le);

    seq_stats_init(&welford);
    bias_foo(&welford, &stats.oi[0][0], 2.0/(double)stats.n, 32*32);
    seq_stats_print(&welford);
#if 1
    printf("\n");
#else
    gof_print(&stats.le);
#endif    

    bias_clear_counts(&stats);
  }
}


void internal_error(char* msg, uint32_t code)
{
  if (code != 0)
    fprintf(stderr, "internal error: %s (%c)\n", msg, code);
  else
    fprintf(stderr, "internal error: %s\n", msg);
  exit(-1);
}

void print_error(char* msg)
{
  fprintf(stderr, "error: %s\n", msg);
}

void print_warning(char* msg)
{
  fprintf(stderr, "warning: %s\n", msg);
}


void help_options(char* name)
{
  printf("Usage: %s [OPTIONS] FILE\n", name);
  printf("\n"
	 "hacky junk. integer arguments silently accept\n"
	 "non-integer values and treat them like zero.\n"
	 "  --inc       Weyl sequence increment     (32-bit, odd)\n"
	 "  --state     Weyl sequence initial state (32-bit)\n"
	 "  --help\n"
	 "\n");

  exit(0);
}

uint64_t parse_u64(char* str)
{
  char*    end;
  uint64_t val = strtoul(str, &end, 0);

  return val;
}


int main(int argc, char** argv)
{
  uint32_t param_errors = 0;

  if (0) {
    static const uint64_t k[] = {
      0xd1342543de82ef95,   //   5*17*1277*2908441*47750621
      0xaf251af3b0f025b5,   //   5*342349289*7372892873
      0xb564ef22ec7aece5,   //   3^2*13*17*37*5101*34818615649
      0xf7c2ebc08f67f2b5,   //   79*225988494748383163
      0x27bb2ee687b0b0fd,   //   3*954311185259313919
      0x369dea0f31a53f85,   //   3*5*262370600024666923
    };

    for(uint32_t i=0; i<6; i++) {
      uint64_t v = k[i];
      printf("%016lx ",v);
      printb(v,64);
      printf(" : %2u %2i\n", pop_64(v), bit_run_count_64(v));
    }
    return 0;
  }

  
  examine_current(finalize_m0);
  printf("\n");
  examine_current(finalize_m1);
  return 0;

  
  static struct option long_options[] = {
    {"inv",        no_argument,       0,  0},
    {"seq",        no_argument,       0,  4},
    {"phi",        no_argument,       0, 'p'},
    {"inc",        required_argument, 0, 'i'},
    {"state",      required_argument, 0, 's'},
    {"help",       optional_argument, 0, '?'},
    {0,            0,                 0,  0 }
  };
  
  int c;
  
  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "", long_options, &option_index);
    
    if (c == -1)
      break;

    switch (c) {
    case 'i': {
      uint64_t v = parse_u64(optarg);
      
      if (((v & 1)==1) && (v <= 0xffffffff)) {
	weyl_inc = (uint32_t)v;
      }
      else {
	print_error("--inc must be odd 32-bit integer");
	param_errors++;
      }
    }
    break;

    case 'p': weyl_inc = 0x9e3779b9; break;
    
    case 's': {
      uint64_t v = parse_u64(optarg);
      
      if (v <= 0xffffffff) {
	weyl_state = (uint32_t)v;
      }
      else {
	print_error("--state must be a 32-bit integer");
	param_errors++;
      }
    }
    break;
    
#if 0      
    case 0: case 1: case 2:
      {
	uint64_t v = parse_u64(optarg);
	v <<= (10*c);
	if (v != 0)
	  num_blocks = v;
      }
      break;

    case 3: case 4: fill_type = (uint32_t)(c-3); break;

    case 5: case 6: case 7:
      sample_type = (uint32_t)(c-5);

      if (optarg) {
	uint64_t v = parse_u64(optarg);
	sample_state.inc = v|1;

	if ((v & 1)==0) {
	  fprintf(stderr, "warning: increment must be odd. setting=0x%016lx\n", v|1); 
	}
      }
      break;

    case 'h':
      if (optarg) {
	bit_finalizer = get_hash(optarg);
	break;
      }
      print_xorshift_mul_3();
      return 0;
      
    case 's': sample_state.state = parse_u64(optarg);  break;
    case 'p': sample_state.inc   = 0x9e3779b97f4a7c15; break;
    case '?': help_options(argv[0]);                   break;
#endif      
      
    default:
      printf("internal error: what option? %c (%u)\n", c,c);
    }
  }

  if (param_errors) { } // yeah..something
  
  // all arguments should have been consumed
  if (optind == argc) {

    test_table();
    
    return 0;
  }

  
  
  print_error("expected a single filename after the options");

  return -1;
}

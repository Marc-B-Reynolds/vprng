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

void printb(uint64_t v, uint32_t b)
{
  uint64_t m = UINT64_C(1)<<(b-1);
  do { printf("%c", (v & m)!=0 ? '1':'.'); m >>= 1; } while (m != 0);
}


typedef struct {
  uint32_t oi[64][64];  // per bit position counts
  uint32_t pop[65];     // population count
  uint32_t oiI[64];     // output bit
  uint32_t oiO[64];     // input bit
  uint32_t n;           // number of inputs processed
} sac_64_t;

void sac_clear_64(sac_64_t* s)
{
  memset(s, 0, sizeof(s[0]));
}

static inline void sac_core(sac_64_t* s, uint64_t (*f)(uint64_t), uint64_t x)
{
  // compute the hash for input x (from sampling method)
  uint64_t h = f(x);
  
  // flip each bit of input 'x'
  for (uint32_t i=0; i<64; i++) {
    uint64_t t   = h ^ f(x ^ (UINT64_C(1)<<i));
    uint32_t pop = pop_64(t);
    
    // gather bit-by-bit counts
    for (uint32_t j=0; j<64; j++) {
      uint64_t b = (t >> j) & 1;
      s->oi[i][j] += (uint32_t)b;
    }

    // gather population count data
    s->oiI[i] += pop;
    s->pop[pop]++;
  }
}


void sac_add(sac_64_t* s, uint64_t (*f)(uint64_t), uint32_t n)
{
  // temp hack
  uint64_t du = UINT64_C(0x9e3779b97f4a7c15);
  uint64_t u  = du * s->n;
  
  s->n += n;
  
  for(uint32_t i=0; i<n; i++) {
    sac_core(s,f,u);
    u += du;
  }
}

//------------------------------------------------

#define SAC_LEN (1<<20)

static inline void dumb_vle_sac(char* name, uint64_t (*f)(uint64_t))
{
  char filename[256];

  snprintf(filename, sizeof(filename)-1, "%s.dat", name);

  uint32_t data[64*64] = {0};

  FILE*  file = fopen(filename, "wb");

  static const float s = 2.f/(float)SAC_LEN;
  
  if (file) {
    uint64_t x = 0;

    // walk a set of base values
    for(uint32_t i=0; i<SAC_LEN; i++) {
      x = 0x9e3779b97f4a7c15*(uint64_t)i;
      //x= (uint64_t)i;
      
      uint64_t  b = 1;
      uint64_t  h = f(x);
      uint32_t* d = data;

      // 
      for (uint32_t i=0; i<64; i++) {
	uint64_t t = h ^ f(x ^ b);

	b <<= 1;
	
	for (uint32_t j=0; j<64; j++) {
	  d[0] += (t & 1);
	  t   >>= 1;
	  d++;
	}
      }
    }

    // compute the bias
    float bias[64*64] = {0};
    float bias_max = 0.f;
    seq_stats_t stats;

    seq_stats_init(&stats);
    
    for(uint32_t i=0; i<64*64; i++) {
      float t = fmaf((float)data[i],s, -1.0);
      bias[i] = t;
      t = fabsf(t);

      seq_stats_add(&stats,t);
      bias_max = (bias_max >= t) ? bias_max : t;
    }

    printf("%s : |bias| = %10.8f ±% 10.8f : max = %f\n", name,
	   stats.m, seq_stats_stddev(&stats),
	   bias_max);
    fwrite(bias, 1, sizeof(bias), file);
    fclose(file);


    snprintf(filename, sizeof(filename)-1, "%s_i.dat", name);
    file = fopen(filename, "wb");

    if (file) {
      // isolate & rescale values > mean + stddev
      float limit = (float)(stats.m + seq_stats_stddev(&stats));
      float scale = (float)(1.0/(bias_max-stats.m));
      float mean  = (float)stats.m;
      
      for(uint32_t i=0; i<64*64; i++) {
	float t = fabsf(bias[i]);
	
	t  = (t <= limit) ? mean : t;
	t -= mean;
	t  = (bias[i]>=0) ? t : -t;
	t *= scale;
	
	bias[i] = t;
      }
      fwrite(bias, 1, sizeof(bias), file);
      fclose(file);
    }
    else
      fprintf(stderr, "error: couldn't open '%s'\n", filename);
  }  
  else
    fprintf(stderr, "error: couldn't open '%s'\n", filename);
}

//------------------------------------------------


const uint32_t k[] =
{
  // lcg/mcg
  0x915f77f5, // 1..1...1.1.11111.111.1111111.1.1 : 21  9  0
  0x93d765dd, // 1..1..1111.1.111.11..1.111.111.1 : 20 10  1 -
  0xadb4a92d, // 1.1.11.11.11.1..1.1.1..1..1.11.1 : 17 13  2
  0xa13fc965, // 1.1....1..11111111..1..1.11..1.1 : 17  9  3
  0x8664f205, // 1....11..11..1..1111..1......1.1 : 13  8  4
  0xcf019d85, // 11..1111.......11..111.11....1.1 : 15  7  5
  0xae3cc725, // 1.1.111...1111..11...111..1..1.1 : 17  9  6
  0x9fe72885, // 1..11111111..111..1.1...1....1.1 : 17  8  7
  0xae36bfb5, // 1.1.111...11.11.1.1111111.11.1.1 : 21 10  8
  0x82c1fcad, // 1.....1.11.....1111111..1.1.11.1 : 16  8  9 -
  0xac564b05, // 1.1.11...1.1.11..1..1.11.....1.1 : 14 11  10 -
  0x01c8e815, // .......111..1...111.1......1.1.1 : 11  7  11
  0x01ed0675, // .......1111.11.1.....11..111.1.1 : 14  7  12
  0x2c2c57ed, // ..1.11....1.11...1.1.111111.11.1 : 17  9  13
  0x5f356495, // .1.11111..11.1.1.11..1..1..1.1.1 : 17 11  14
  0x2c9277b5, // ..1.11..1..1..1..111.1111.11.1.1 : 17 10  15

  0x21f0aaad, // 
  0xf35a2d97, // 
  0x603a32a7,
  0x5a522677,
};


static inline int32_t  asr_s32(int32_t  x, uint32_t n) { return x >> (n & 0x1f); }
static inline uint32_t asr_u32(uint32_t x, uint32_t n) { return (uint32_t)asr_s32((int32_t )x, n); }
static inline uint32_t ctz_32(uint32_t x) { return (x!=0) ? (uint32_t)__builtin_ctz(x)  : 32; }

static inline uint32_t pop_next_32(uint32_t x)
{
  uint32_t t = x + (x & -x);
  x = x & ~t;
  x = asr_u32(x, ctz_32(x));
  x = asr_u32(x, 1);
  return t^x;
}

static void spew(void)
{
}



static inline u32x8_t og_mix(u64x4_t x)
{
  static const u32x8_t m0 =
    { 0x21f0aaad, 0x603a32a7, 0x21f0aaad, 0x97219aad,
      0xb237694b, 0x8ee0d535, 0xdc63b4d3, 0x93f2552b
    };
  
  static const u32x8_t m1 = {
    0xf35a2d97, 0x5a522677, 0xd35a2d97, 0xab46b735,
    0xeb5b4593, 0x5dc6b5af, 0x2c32b9a9, 0x959b4a4d
  };
  
  x ^= x >> 33;
  
  x ^= x >> 16; x = vprng_mix_mul(x,m0);
  x ^= x << 15; x = vprng_mix_mul(x,m1);
  x ^= x >> 17;
  
  return vprng_cast_u32(x);
}



#if 0
// |bias| = 0.00087705 ± 0.00080137 : max = 0.011385
static const uint32_t a0 = 0b01010100111001010101100110011001;
static const uint32_t a1 = 0b00101100100100110111011110110101;
static const uint32_t a2 = 0b00111010100110101010100110101011;
static const uint32_t a3 = 0b01000101000100110110100110110101;
#elif 0
// |bias| = 0.00088371 ± 0.00081792 : max = 0.009584 (humm)
static const uint32_t a0 = 0b01010100111001010101100110011001;
static const uint32_t a1 = 0b00101100100100110111011010110101;
static const uint32_t a2 = 0b00111010100110101010100110101011;
static const uint32_t a3 = 0b01000101000100110110100110110101;
#else
static const uint32_t a0 = 0b01010100111001010101100110011001;
static const uint32_t a1 = 0b00101100100100111111011010110101;
static const uint32_t a2 = 0b00111010100110101010100110101001;
static const uint32_t a3 = 0b01000101000100110110100110110101;
#endif

// |bias| = 0.00101875 ± 0.00115915 : max = 0.012527
static const uint32_t b0 = 0b11110011010110100010110110010111;
static const uint32_t b1 = 0b10101101101101001010100100101011;
static const uint32_t b2 = 0b10000010110000101111110010101101;
static const uint32_t b3 = 0b10101100010101100100101100000111;

// |bias| = 0.00101763 ± 0.00106683 : max = 0.011875
static const uint32_t c0 = 0b10010011010101110110010111011101; // 935765dd
static const uint32_t c1 = 0b10101101101101001010100100101101; // adb4a92d
static const uint32_t c2 = 0b10000010110000011111110010101101; // 82c1fcad
static const uint32_t c3 = 0b10101100010101100100101100000111; // ac564b07

// |bias| = 0.00085375 ± 0.00079878 : max = 0.017200
static const uint32_t d0 = 0b01100011011010001010101010101101;
static const uint32_t d1 = 0b10101110110001010101000101001011;
static const uint32_t d2 = 0b10000010111000101111001010101101;
static const uint32_t d3 = 0b01011010010101001001110100010111;


// 2 product & first shift 33
// |bias| = 0.00089735 ± 0.00081506 : max = 0.009876
// |bias| = 0.00101875 ± 0.00115915 : max = 0.012527
// |bias| = 0.00101763 ± 0.00106683 : max = 0.011875
// |bias| = 0.00085375 ± 0.00079878 : max = 0.017200

static const uint32_t a4 = 0b11111111111111111111111111111111;
static const uint32_t a5 = 0b00001010101010111001010010110101;
static const uint32_t b4 = 0b00000000000000000000000000000001;
static const uint32_t b5 = 0b00000000000000000000000000000001;
static const uint32_t c4 = 0b00000000000000000000000000000001;
static const uint32_t c5 = 0b00000000000000000000000000000001;
static const uint32_t d4 = 0b00000000000000000000000000000001;
static const uint32_t d5 = 0b00000000000000100101101000100101;

#if 0
static const u32x8_t m0 = {a0,a1,b0,b1,c0,c1,d0,d1};
static const u32x8_t m1 = {a2,a3,b2,b3,c2,c3,d2,d3};
static const u32x8_t m2 = {a4,a5,b4,b5,c4,c5,d4,d5};
#else
static const u32x8_t m0 = {d0,d1,b0,b1,c0,c1,a0,a1};
static const u32x8_t m1 = {d2,d3,b2,b3,c2,c3,a2,a3};
static const u32x8_t m2 = {d4,d5,b4,b5,c4,c5,a4,a5};
#endif

static inline u32x8_t local_mix(u64x4_t x)
{
  x ^= x >> 33;
  
  x ^= x >> 16; x = vprng_mix_mul(x,m0);
  x ^= x << 16; x = vprng_mix_mul(x,m1);
  x ^= x >> 16; x = vprng_mix_mul(x,m2);

  x ^= x >> 32;
  
  return vprng_cast_u32(x);
}

#if 0
#include <x86intrin.h>
// just goofing. would need a base (or combined state) where a 128-bit permutation
// preserves uniformity.
static const __m128i key_lo = {INT64_C(0x85a308d3243f6a88), INT64_C(0x0370734413198a2e)};  // no
static const __m128i key_hi = {INT64_C(0x299f31d0a4093822), INT64_C(0xec4e6c89082efa98)};  // double no

static inline u32x8_t aes_mix(u64x4_t x)
{
  x ^= x >> 32;
  
  __m256i v;
  memcpy(&v, &x, 32);

  __m128i lo = _mm256_castsi256_si128(v);
  __m128i hi = _mm256_extracti128_si256(v, 1);

  // probably 2 rounds (w different key...just speculating)
  lo = _mm_aesenc_si128(lo, key_lo);
  hi = _mm_aesenc_si128(hi, key_lo);
  lo = _mm_aesenc_si128(lo, key_hi);
  hi = _mm_aesenc_si128(hi, key_hi);
  lo = _mm_aesenc_si128(lo, key_lo);
  hi = _mm_aesenc_si128(hi, key_lo);

  v  = _mm256_set_m128i(hi,lo);

  memcpy(&x, &v, 32);

  return vprng_cast_u32(x);
}
#endif

/*
  // original
  v0.0.1_0.dat : |bias| = 0.00099809 ± 0.00152802 : max = 0.049612
  v0.0.1_1.dat : |bias| = 0.00101763 ± 0.00106683 : max = 0.011875
  v0.0.1_2.dat : |bias| = 0.00101875 ± 0.00115915 : max = 0.012527
  v0.0.1_3.dat : |bias| = 0.00113265 ± 0.00178342 : max = 0.030331
  
  // middle to 15
  v0.0.1_0.dat : |bias| = 0.00116757 ± 0.00233328 : max = 0.058140
  v0.0.1_1.dat : |bias| = 0.00110669 ± 0.00134701 : max = 0.028269
  v0.0.1_2.dat : |bias| = 0.00116897 ± 0.00154916 : max = 0.026316
  v0.0.1_3.dat : |bias| = 0.00135839 ± 0.00258413 : max = 0.039268
*/


static inline u32x8_t local_nl_mix(u64x4_t x)
{
  x ^= x >> 33;
  
  x ^= x >> 16; x = vprng_mix_mul(x,m0);
  x ^= x << 16; x = vprng_mix_mul(x,m1);
  x ^= x >> 16;

  x ^= x >> 32;

  return vprng_cast_u32(x);


  x ^= x >> 32;
  x ^= x >> 16; x = vprng_mix_mul(x,m0);
  x ^= x << 16; x = vprng_mix_mul(x,m1);
  x ^= x >> 32;
  x ^= vprng_cast_u64((vprng_cast_u32(x)*vprng_cast_u32(x)) & UINT32_C(-2));

  
  return vprng_cast_u32(x);
}


typedef struct {
  uint64_t m0,m1;
  uint8_t  s0,s1,s2;
} foo_t;

const foo_t foo[] =
{
  // stripped mined from mini_driver_testu01
  {.s0=31, .m0=0x69b0bc90bd9a8c49, .s1=27, .m1=0x3d5e661a2a77868d, .s2=30}, // 0.00077386 ± 0.00059516 : 0.004635 (mix06)
  {.s0=30, .m0=0xbf58476d1ce4e5b9, .s1=27, .m1=0x94d049bb133111eb, .s2=31}, // 0.00076417 ± 0.00058683 : 0.004410 (mix13)
  {.s0=29, .m0=0x3cd0eb9d47532dfb, .s1=26, .m1=0x63660277528772bb, .s2=33}, // 0.00077634 ± 0.00059283 : 0.004045 (mix12)
  {.s0=33, .m0=0x64dd81482cbd31d7, .s1=31, .m1=0xe36aa5c613612997, .s2=31}, // 0.00077657 ± 0.00058873 : 0.004025 (mix02)
  {.s0=31, .m0=0x79c135c1674b9add, .s1=29, .m1=0x54c77c86f6913e45, .s2=30}, // 0.00078432 ± 0.00058382 : 0.003956 (mix05)
  {.s0=30, .m0=0x4be98134a5976fd3, .s1=29, .m1=0x3bc0993a5ad19a13, .s2=31}, // 0.00078181 ± 0.00059679 : 0.003954 (mix14)
  {.s0=33, .m0=0x62a9d9ed799705f5, .s1=28, .m1=0xcb24d0a5c88c35b3, .s2=32}, // 0.00076985 ± 0.00058887 : 0.003874 (mix04)
  {.s0=32, .m0=0xdaba0b6eb09322e3, .s1=32, .m1=0xdaba0b6eb09322e3, .s2=32}, // 0.00077389 ± 0.00057988 : 0.003719 (lea)
  {.s0=33, .m0=0xc2b2ae3d27d4eb4f, .s1=29, .m1=0x165667b19e3779f9, .s2=32}, // 0.00077169 ± 0.00058688 : 0.003706 (xxhash)
  {.s0=30, .m0=0x294aa62849912f0b, .s1=28, .m1=0x0a9ba9c8a5b15117, .s2=31}, // 0.00079825 ± 0.00059812 : 0.003685 (mix08)
  {.s0=33, .m0=0xff51afd7ed558ccd, .s1=33, .m1=0xc4ceb9fe1a85ec53, .s2=33}, // 0.00077699 ± 0.00058454 : 0.003626 (murmur3)
  {.s0=30, .m0=0x16a6ac37883af045, .s1=26, .m1=0xcc9c31a4274686a5, .s2=32}, // 0.00078278 ± 0.00059159 : 0.003601 (mix07)
  {.s0=31, .m0=0x7fb5d329728ea185, .s1=27, .m1=0x81dadef4bc2dd44d, .s2=33}, // 0.00076907 ± 0.00057240 : 0.003584 (mix01)
  {.s0=30, .m0=0xe4c7e495f4c683f5, .s1=32, .m1=0xfda871baea35a293, .s2=33}, // 0.00077567 ± 0.00058542 : 0.003490 (mix10)
  {.s0=32, .m0=0x4cd6944c5cc20b6d, .s1=29, .m1=0xfc12c5b19d3259e9, .s2=32}, // 0.00076962 ± 0.00059438 : 0.003395 (mix09)
  {.s0=27, .m0=0x97d461a8b11570d9, .s1=28, .m1=0x02271eb7c6c4cd6b, .s2=32}, // 0.00078008 ± 0.00058315 : 0.003399 (mix11)

  {.s0=31, .m0=0x99bcf6822b23ca35, .s1=30, .m1=0x14020a57acced8b7, .s2=33}, // 0.00077872 ± 0.00059169 : 0.003340 (mix03)

  //{.s0=41, .m0=0x09bc00822b20ca35, .s1=30, .m1=0x10020a07accedfb7, .s2=33}, // hobbled
};



static inline u32x8_t mix_64(u64x4_t x)
{
  // grab last ATM
  foo_t def = foo[sizeof(foo)/sizeof(foo[0])-1];
  
  x ^= x >> def.s0; x *= def.m0;
  x ^= x >> def.s1; x *= def.m1;
  x ^= x >> def.s2;

  return vprng_cast_u32(x);
}


// need 8x32-bit tester
static inline u32x8_t mix_32(u64x4_t x)
{
static const u32x8_t vpcg_mul_m0 = {0x21f0aaad,0xa52fb2cd,0x7feb352d,0x4bdc9aa5,0xac10d4eb,0xdf892d4b,0x462daaad,0x4ffcab35};
static const u32x8_t vpcg_mul_m1 = {0x735a2d97,0x551e4d49,0x846ca68b,0x2729b469,0x9d51b169,0x3c2da6b3,0x0a36c95d,0xe98db28b};

  u32x8_t u = vprng_cast_u32(x);
  
  u ^= u >> 16; u *= vpcg_mul_m0;
  u ^= u >> 15; u *= vpcg_mul_m1;
  u ^= u >> 15;

  return u;
}





// temp hack
static inline uint64_t hack_hash(uint64_t x, uint32_t c)
{
  u64x4_t v = {x,x,x,x};

  //v= vprng_cast_u64(mix_32(v));
  //v= vprng_cast_u64(mix_64(v));
  //v= vprng_cast_u64(vprng_mix(v));
  v= vprng_cast_u64(local_mix(v));
  //v= vprng_cast_u64(og_mix(v));
  //v= vprng_cast_u64(local_nl_mix(v));

  return v[c];
}

uint64_t hash0(uint64_t x) { return hack_hash(x,0); }
uint64_t hash1(uint64_t x) { return hack_hash(x,1); }
uint64_t hash2(uint64_t x) { return hack_hash(x,2); }
uint64_t hash3(uint64_t x) { return hack_hash(x,3); }

int main(void)
{
  //sac_64_t s = {0};
  //sac_add(&s, hash, 0xffff);

  dumb_vle_sac("v0.0.1_0",  hash0);
  dumb_vle_sac("v0.0.1_1",  hash1);
  dumb_vle_sac("v0.0.1_2",  hash2);
  dumb_vle_sac("v0.0.1_3",  hash3);

  return 0;
}



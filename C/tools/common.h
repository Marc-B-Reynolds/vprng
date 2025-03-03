#pragma once


#define LENGTHOF(X) (sizeof(X)/sizeof(X[0]))

static inline uint32_t ctz_64(uint64_t x) { return (x!=0) ? (uint32_t)__builtin_ctzll(x) : 64; }

static inline uint32_t pop_64(uint64_t x) { return (uint32_t)__builtin_popcountll(x);  }
static inline uint32_t pop_32(uint32_t x) { return (uint32_t)__builtin_popcount(x);  }
static inline uint32_t bit_run_count_32(uint32_t x) { return pop_32(x & (x^(x>>1))); }
static inline uint32_t bit_run_count_64(uint64_t x) { return pop_64(x & (x^(x>>1))); }


// Welford's method for streaming mean/variance/stdev
typedef struct { double n,m,s,min,max; } seq_stats_t;

static inline void seq_stats_init(seq_stats_t* d)
{
  memset(d,0,sizeof(seq_stats_t));
  d->min = 0x1.fffffffffffffp+1023;
  d->max = -d->min;
}

static inline void seq_stats_add(seq_stats_t* d, double v)
{
  double x = v;
  
  d->n += 1.0;

  double m  = d->m;
  double s  = d->s;
  double dm = x-m;
  
  d->m  = m + dm/d->n;
  d->s  = fma(dm, x-d->m, s);
  d->min = (v >= d->min) ? d->min : v;
  d->max = (v <= d->max) ? d->max : v;
}

static inline double seq_stats_mean(seq_stats_t* d)     { return d->m; }
static inline double seq_stats_variance(seq_stats_t* d) { return d->s/(d->n-1.0); }
static inline double seq_stats_stddev(seq_stats_t* d)   { return sqrt(seq_stats_variance(d)); }

static inline void seq_stats_print(seq_stats_t* d)
{
  printf("mean=%f stddev=%f min=%f max=%f",
	 d->m,
	 //seq_stats_variance(d),
	 seq_stats_stddev(d),
	 //(uint32_t)d->n,
	 d->min, d->max
	 );
}

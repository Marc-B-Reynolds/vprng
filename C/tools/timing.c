
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <math.h>
#include <stdio.h>

// temp hack
#include <x86intrin.h>

// if defined to aggressively look for "peak" time. 
#define TRIM_TIMINGS

const bool time_cycles = true;


#define LENGTHOF(X) (sizeof(X)/sizeof(X[0]))

#define VPRNG_IMPLEMENTATION
#ifndef VPRNG_INCLUDE
#include "vprng.h"
#else
#include VPRNG_INCLUDE
#endif

#define HEADER     "\033[95m"
#define OKBLUE     "\033[94m"
#define OKCYAN     "\033[96m"
#define OKGREEN    "\033[92m"
#define WARNING    "\033[93m"
#define FAIL       "\033[91m"
#define ENDC       "\033[0m"
#define BOLD       "\033[1m"
#define UNDERLINE  "\033[4m"


// number of 256 bit chucks to produce at a time
//#define BUFFER_LEN 1024
#define BUFFER_LEN 2048

const  int32_t time_trials =  2048;   // number of trials

alignas(32) char raw_buffer[32*BUFFER_LEN];

// 
char*  time_string_cycles = "mean ± std (cycles) ";
char*  time_string_ns     = "  mean ± std (ns)   ";
char*  time_string;

//const double time_scale = (1.0)/((double)sizeof(raw_buffer));
const double time_scale = (1.0)/((double)BUFFER_LEN);
//const double time_scale = 1.0;

static inline uint64_t time_get(void)
{
  if (time_cycles)
    return _rdtsc();

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (1000*1000*1000*(uint64_t)ts.tv_sec + (uint64_t)ts.tv_nsec);
}

static inline double doubletime(void)
{
  return (double)time_get();
}

// Welford's method for streaming mean/variance/stdev
// --- just being lazy. no "streaming" is involved
// here.
typedef struct { double n,m,s; } seq_stats_t;

static inline void seq_stats_init(seq_stats_t* d)
{
  memset(d,0,sizeof(seq_stats_t));
}

void seq_stats_add(seq_stats_t* d, double x)
{
  d->n += 1.0;

  double m  = d->m;
  double s  = d->s;
  double dm = x-m;
  
  d->m = m + dm/d->n;
  d->s = s + dm*(x-d->m);
}

static inline double seq_stats_mean(seq_stats_t* d)     { return d->m; }
static inline double seq_stats_variance(seq_stats_t* d) { return d->s/(d->n-1.0); }
static inline double seq_stats_stddev(seq_stats_t* d)   { return sqrt(seq_stats_variance(d)); }

int cmp_u64(const void * a, const void * b)
{
  return ( *(uint64_t*)a > *(uint64_t*)b );
}

u32x8_t u32x8_junk = {0};
f32x8_t f32x8_junk = {0};
f64x4_t f64x4_junk = {0};

__attribute__((noinline)) void nop(__attribute__((unused))void* none)
{
}

__attribute__((noinline)) void fill(__attribute__((unused))void* none)
{
  memset(raw_buffer, 0, sizeof(raw_buffer));
}

__attribute__((noinline)) void vprng_fill_u32(vprng_t* prng)
{
  u32x8_t* d = (u32x8_t*)raw_buffer;

  for(uint32_t i=0; i<BUFFER_LEN; i++) { d[i] = vprng_u32x8(prng); }
}

__attribute__((noinline)) void vprng_run_u32(vprng_t* prng)
{
  u32x8_t t = u32x8_junk;
  for(uint32_t i=0; i<BUFFER_LEN; i++) { t ^= vprng_u32x8(prng); }
  u32x8_junk = t;
}

__attribute__((noinline)) void vprng_fill_f32(vprng_t* prng)
{
  f32x8_t* d = (f32x8_t*)raw_buffer;

  for(uint32_t i=0; i<BUFFER_LEN; i++) { d[i] = vprng_f32x8(prng); }
}

__attribute__((noinline)) void vprng_run_f32(vprng_t* prng)
{
  f32x8_t t = f32x8_junk;
  for(uint32_t i=0; i<BUFFER_LEN; i++) { t += vprng_f32x8(prng); }
  f32x8_junk = t;
}

__attribute__((noinline)) void cvprng_fill_u32(cvprng_t* prng)
{
  u32x8_t* d = (u32x8_t*)raw_buffer;

  for(uint32_t i=0; i<BUFFER_LEN; i++) { d[i] = cvprng_u32x8(prng); }
}

__attribute__((noinline)) void cvprng_fill_f32(cvprng_t* prng)
{
  f32x8_t* d = (f32x8_t*)raw_buffer;

  for(uint32_t i=0; i<BUFFER_LEN; i++) { d[i] = cvprng_f32x8(prng); }
}

typedef struct {
  char* name;
  void (*f)(void*);
  void* state;
} func_entry_t;

vprng_t  vprng;
cvprng_t cvprng;

func_entry_t func_table[] =
  {
  //{.name = "nop",            .f=(void*)nop,             .state=0},
    {.name = "memset",         .f=(void*)fill,            .state=0},
    {.name = "run vprng  u32", .f=(void*)vprng_run_u32,   .state=&vprng},
    {.name = "mem vprng  u32", .f=(void*)vprng_fill_u32,  .state=&vprng},
    {.name = "run vprng  f32", .f=(void*)vprng_run_f32,   .state=&vprng},
    {.name = "mem vprng  f32", .f=(void*)vprng_fill_f32,  .state=&vprng},
    {.name = "mem cvprng u32", .f=(void*)cvprng_fill_u32, .state=&cvprng},
    {.name = "mem cvprng f32", .f=(void*)cvprng_fill_f32, .state=&cvprng},
  };


void timing_run(func_entry_t* entry, uint64_t data[static time_trials])
{
  struct timespec sleep_req = {0, 100000};

  for(uint32_t n=0; n<time_trials; n++) {
    uint64_t tdata[3];

    // run three times and take median
    for(uint32_t i=0; i<3; i++) {
      atomic_thread_fence(memory_order_seq_cst);
      uint64_t t0 = time_get();
      
      entry->f(entry->state);
      
      atomic_thread_fence(memory_order_seq_cst);
      
      uint64_t t1 = time_get();
      tdata[i]    = t1-t0;
    
      nanosleep(&sleep_req, NULL);
    }

    // this "trial" is the fastest sample
    qsort(tdata, 3, sizeof(uint64_t), cmp_u64);  // LOL! 3 elements
    data[n] = tdata[0];
  }

  // for lazy min/median/max
  qsort(data, time_trials, sizeof(uint64_t), cmp_u64);
}

double timing_gather(seq_stats_t* s, uint64_t data[static time_trials])
{
  seq_stats_init(s);

#if defined(TRIM_TIMINGS)
  // everything is branchfree and attempting to spitball
  // "peak" peformance of all methods.
  uint32_t e = time_trials >> 1;
#else
  uint32_t e = time_trials;
#endif  

  for(uint32_t n=0; n<e; n++)
    seq_stats_add(s, (double)data[n] * time_scale);

  // no need to be careful here methinks
  double mean = s->m;
  double sum  = 0.0;

  for(uint32_t n=0; n<e; n++) {
    double v = (double)data[n] * time_scale;
    double d = mean-v;
    sum = fma(d,d,sum);
  }

  return sum/mean;
}



void timing_test(func_entry_t* entry, int len)
{
  //uint32_t rerun = 5;//time_rerun;
  uint64_t data[time_trials];
  seq_stats_t stats;

  printf(BOLD "%s" ENDC " (%s)\n", VPRNG_NAME, VPRNG_VERSION_STR);
  
  printf("┌───────────────────┬"
         "────────────────────────────┬"
         "────────────────┬"
         "─────────┬"
         "─────────┬"
         "─────────┐"
         "\n");
  
  printf(WARNING "│ %-18s│ %27s │ %14s │%8s │%8s │%8s │\n" ENDC,
         "function",
         time_string,
         "std/mean",
         "min ",
         "median",
         "max "
         );
  
  printf("├───────────────────┼"
         "────────────────────────────┼"
         "────────────────┼"
         "─────────┼"
         "─────────┼"
         "─────────┤"
         "\n");

  while(len--) {
    for(uint32_t i=0; i<1; i++) {
      printf("│ %-18s", entry->name);
      
      timing_run(entry, data);
      timing_gather(&stats, data);
      
      double std = sqrt(seq_stats_variance(&stats));
      
      printf("│%13.8f ±% 13.8f│ (1 ± %-8f) │%9.5f│%9.5f│%9.5f│",
	     stats.m, std,
	     std/stats.m,
	     (double)data[0] * time_scale,
	     (double)data[time_trials>>1] * time_scale,
	     (double)data[time_trials-1] * time_scale
	     );
      
      printf("\n");
    }
    
    entry++;
  }

  printf("└───────────────────┴"
         "────────────────────────────┴"
         "────────────────┴"
         "─────────┴"
         "─────────┴"
         "─────────┘\n");
}

//********************************************************


int set_thread_max_priority(pthread_t thread)
{
  struct sched_param param;
  int policy;

  if (pthread_getschedparam(thread, &policy, &param) == 0) {
    policy               = SCHED_FIFO;
    param.sched_priority = sched_get_priority_max(policy);
    
    if (pthread_setschedparam(thread, policy, &param) == 0)
      return true;
  }
  
  return false;
}

int main(void)
{
  vprng_global_id_set(1);

  vprng_init(&vprng);
  cvprng_init(&cvprng);

  if (time_cycles) {
    time_string = time_string_cycles;
  }
  else {
    time_string = time_string_ns;
  }
  
  if (!set_thread_max_priority(pthread_self()))
    printf("failed to up thread priority\n");

  timing_test(func_table,  LENGTHOF(func_table));

  printf("\n"
	 "time is per 32 byte chunk\n"
	 " run = temp variable accumulation\n"
	 " mem = buffer fill\n");

#if defined(TRIM_TIMINGS)
  printf("\n"
	 WARNING "Timing is aggressively looking for peak."
	 ENDC    " (undefine TRIM_TIMINGS to disable)\n");
#endif  

  printf("\n");

  return 0;
}

// for dump PRNG output to either a file or stdout for statistical testing or regression testing

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wchar.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <inttypes.h>

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

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

// true if only dumping informtion about what the run would do
bool dry_run = false;
bool cvprng_hobble_state = false;

uint64_t global_id = 1;
uint64_t test_id   = UINT64_C(-1);

// dump run information to stderr (for user info and/or capture by calling scripts)
// not stdio since that can be consumed by a piped executable
void test_banner_i(char* str)
{
  fprintf(stderr, "%s (%s) id=%" PRIu64, str, VPRNG_VERSION_STR, global_id);

  if (test_id != UINT64_C(-1))
    fprintf(stderr, " (from test-id=%" PRIu64 ") ", test_id);
}


void test_banner(char* str, vprng_t* prng)
{
  test_banner_i(str);
  fprintf(stderr, "\n");

  u64x4_t inc = vprng_inc(prng);
  
  // temp hack
  fprintf(stderr,
	  "{%016" PRIx64
	  ",%016" PRIx64
	  ",%016" PRIx64
	  ",%016" PRIx64
	  "}\n\n", inc[0],inc[1],inc[2],inc[3]);
  
  if (dry_run) exit(0);
}

void test_banner_2(char* str, uint32_t i)
{
  test_banner_i(str);
  fprintf(stderr, "channel=%u\n", i);

  if (dry_run) exit(0);
}

uint64_t parse_u64(char* str)
{
  char*    end;
  uint64_t val = strtoul(str, &end, 0);

  return val;
}

void internal_error(char* msg, uint32_t code)
{
  fprintf(stderr, FAIL "internal error: " ENDC  " %s", msg);

  if (code != 0)
    fprintf(stderr, " (%x)", code);

  fprintf(stderr, "\n");
}

void print_error(char* msg)
{
  fprintf(stderr, FAIL "error:" ENDC " %s\n", msg);
}

void print_warning(char* msg)
{
  fprintf(stderr, WARNING "warning:" ENDC " %s\n", msg);
}

// move banner junk in here and add any init time hobbling
void wrap_vprng_init(vprng_t* prng)
{
  vprng_init(prng);
}

void wrap_cvprng_init(cvprng_t* prng)
{
  cvprng_init(prng);
}


// number of 256 bit chucks to produce at a time
#define BUFFER_LEN 1024

u32x8_t buffer[BUFFER_LEN];

static_assert(sizeof(buffer) == BUFFER_LEN*32, "because mistakes were made");

// vprng all 256-bit output
void spew_all(FILE* file, uint64_t n)
{
  vprng_t prng;
  size_t  t;

  wrap_vprng_init(&prng);
  test_banner(VPRNG_NAME, &prng);

  // note: it's on purpose with "n=0" is to run until killed (ditto other loops)
  while(--n) {
    vprng_block_fill_u32(BUFFER_LEN, buffer, &prng);
    t = fwrite(buffer, 1, sizeof(buffer), file);
    if (t == sizeof(buffer)) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

// cvprng all 256-bit output
void cspew_all(FILE* file, uint64_t n)
{
  cvprng_t prng;
  size_t   t;

  wrap_cvprng_init(&prng);
  test_banner("c" VPRNG_NAME, &prng.base);

  while(--n) {
    cvprng_block_fill_u32(BUFFER_LEN, buffer, &prng);
    t = fwrite(buffer, 1, sizeof(buffer), file);
    if (t == sizeof(buffer)) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

// vprng: single 32-bit channel 'c' output
static inline void spew_channel_32(FILE* file, uint64_t n, uint32_t c)
{
  uint32_t data[BUFFER_LEN];
  vprng_t prng;
  size_t  t;

  static_assert(sizeof(data) == BUFFER_LEN*4, "because mistakes were made");
  
  wrap_vprng_init(&prng);
  test_banner_2(VPRNG_NAME " 32-bit lane:", c);

  while(--n) {
    for(uint32_t i=0; i<BUFFER_LEN; i++) {
      u32x8_t r = vprng_u32x8(&prng);
      data[i] = r[c];
    }
    
    t = fwrite(data, 1, sizeof(data), file);
    if (t == sizeof(data)) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

// cprng: single 32-bit channel 'c' output
static inline void cspew_channel_32(FILE* file, uint64_t n, uint32_t c)
{
  uint32_t data[BUFFER_LEN];
  cvprng_t prng;
  size_t   t;

  wrap_cvprng_init(&prng);
  test_banner_2("c" VPRNG_NAME " 32-bit lane:", c);

  while(--n) {
    for(uint32_t i=0; i<BUFFER_LEN; i++) {
      u32x8_t r = cvprng_u32x8(&prng);
      data[i] = r[c];
    }
    
    t = fwrite(data, 1, sizeof(data), file);
    if (t == sizeof(data)) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

// vprng: single 64-bit channel 'c' output
static inline void spew_channel_64(FILE* file, uint64_t n, uint32_t c)
{
  uint64_t data[BUFFER_LEN];
  vprng_t  prng;
  size_t   t;

  static_assert(sizeof(data) == BUFFER_LEN*8, "because mistakes were made");
  
  wrap_vprng_init(&prng);
  test_banner_2(VPRNG_NAME " 64-bit lane:", c);

  while(--n) {
    for(uint32_t i=0; i<BUFFER_LEN; i++) {
      u64x4_t r = vprng_u64x4(&prng);
      data[i] = r[c];
    }
    
    t = fwrite(data, 1, sizeof(data), file);
    if (t == sizeof(data)) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

// cvprng: single 64-bit channel 'c' output
static inline void cspew_channel_64(FILE* file, uint64_t n, uint32_t c)
{
  uint64_t data[BUFFER_LEN];
  cvprng_t prng;
  size_t   t;

  wrap_cvprng_init(&prng);
  test_banner_2("c" VPRNG_NAME " 64-bit lane:", c);

  while(--n) {
    for(uint32_t i=0; i<BUFFER_LEN; i++) {
      u64x4_t r = cvprng_u64x4(&prng);
      data[i] = r[c];
    }
    
    t = fwrite(data, 1, sizeof(data), file);
    if (t == sizeof(data)) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

void channel_error(uint32_t c)
{
  fprintf(stderr, "error: channel (%u) out of range\n", c);
}

enum {
  SMODE    = 0,
  CMODE    = 1,
  MODE_X   = 2,
  MODE_32  = 4,
  SCMODE   = SMODE|MODE_X,
  CCMODE   = CMODE|MODE_X,
  SCMODE32 = SMODE|MODE_32,
  CCMODE32 = CMODE|MODE_32,
};


void help_options(char* name)
{
  printf("Usage: %s OPTIONS FILE\n", name);
  printf("\n"
	 "  --32         32-bit (default: 64-bit)\n"
	 "  --id=N       vprng_global_id_set(N)\n"
	 "  --test-id=N  see README.md\n"
	 "  --cvprng     2 state version\n"
	 "  --channel=N  only channel 'N' output\n"
	 "  --blocks=N   produce N blocks of %u bytes\n"
	 "  --dryrun     dumps out banner information to stderr\n"
	 "  --help       \n"
	 "               \n"
	 VPRNG_NAME " build " VPRNG_VERSION_STR "\n"
	 "\n"
	 "\n", 32*BUFFER_LEN);

  exit(0);
}

int main(int argc, char** argv)
{
  uint32_t mode         = 0;
  uint32_t blocks       = 0;
  uint32_t param_errors = 0;
  FILE *   file         = stdout;

  static struct option long_options[] = {
    {"32",         no_argument,       0, 'w'},
    {"id",         required_argument, 0, 'g'},
    {"test-id",    required_argument, 0, 't'},
    {"cvprng",     no_argument,       0, 'x'},
    {"channel",    required_argument, 0, 'c'},
    {"blocks",     required_argument, 0, 'b'},
    {"dryrun",     no_argument,       0, 'd'},
    {"help",       no_argument,       0, '?'}, 
    {0,            0,                 0,  0 }
  };

  uint32_t channel = 0;

  // Make stdout binary on Windows (only tested in MinGW).
  // NOTE: freopen(NULL, "wb", stdout); suggested
  // in https://www.pcg-random.org/posts/how-to-test-with-practrand.html
  // doesn't seem to work, at least on some machines.
#if defined(_WIN32)
  // if(_setmode(_fileno(stdin ), _O_BINARY)==-1) {fprintf(stderr, "ERROR: _setmode() on stdin failed!\n"); fflush(stderr);}
  if(_setmode(_fileno(stdout), _O_BINARY)==-1) {fprintf(stderr, "ERROR: _setmode() on stdout failed!\n"); fflush(stderr);}
#endif

  while (1) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "", long_options, &option_index);

    if (c == -1) break;

    switch(c)
      {
      case '?': help_options(argv[0]);         break;
      case 'g': global_id = parse_u64(optarg); break;
      case 'w': mode |= MODE_32;               break;
      case 'x': mode |= CMODE;                 break;
      case 'd': dry_run = true;                break;

      case 'b':
	blocks = (uint32_t)parse_u64(optarg);
	break;

      case 'c':
	channel = (uint32_t)parse_u64(optarg);
	mode   |= MODE_X;
	break;

      case 't': {
	// find the n^th (zero based) acceptiable set of additive
	// constants.
	uint64_t tid = parse_u64(optarg);
	  if (tid < 256) {
	    vprng_t t;
	    test_id = tid;
	    vprng_init(&t);
	    
	    while(tid != 0) {
	      vprng_init(&t);
	      tid--;
	    }

	    global_id = vprng_id_get(&t);
   	  }
	  else {
	    print_error("test-id must be on [0,255]");
	    exit(-1);
	  }
	}
	break;
      }
  }

  //--------------------------------------
  
  if (param_errors == 0) {

    // check if we're opening a file
    if (optind == argc-1) {
      char* name = argv[argc-1];
      
      if (access(name, F_OK) != 0) {
	file = fopen(name, "wb");
	if (file) {
	  //setvbuf(file, NULL, _IOFBF, BUFFER_SIZE);
	}
	else {
	  fprintf(stderr, "error: couldn't open file (%s)\n", name);
	  exit(-1);
	}
      }
      else {
	fprintf(stderr, "error: file (%s) already exists\n", name);
	exit(-1);
      }
    }

    // 
    vprng_global_id_set(global_id);

    // TEMP HACK
    if (blocks) blocks++;
    
    switch(mode) {

    case SMODE|MODE_32:
    case SMODE: spew_all(file,blocks);        break;

    case CMODE|MODE_32:
    case CMODE: cspew_all(file,blocks);       break;

    case SMODE|MODE_X: {
      switch(channel) {
      case 0: spew_channel_64(file,blocks,0); break;
      case 1: spew_channel_64(file,blocks,1); break;
      case 2: spew_channel_64(file,blocks,2); break;
      case 3: spew_channel_64(file,blocks,3); break;
      default: channel_error(channel);        break;
      }
    }
    break;

    case CMODE|MODE_X: {
      switch(channel) {
      case 0: cspew_channel_64(file,blocks,0); break;
      case 1: cspew_channel_64(file,blocks,1); break;
      case 2: cspew_channel_64(file,blocks,2); break;
      case 3: cspew_channel_64(file,blocks,3); break;
      default: channel_error(channel);         break;
      }
    }
    break;

    case SMODE|MODE_32|MODE_X: {
      switch(channel) {
      case 0: spew_channel_32(file,blocks,0); break;
      case 1: spew_channel_32(file,blocks,1); break;
      case 2: spew_channel_32(file,blocks,2); break;
      case 3: spew_channel_32(file,blocks,3); break;
      case 4: spew_channel_32(file,blocks,4); break;
      case 5: spew_channel_32(file,blocks,5); break;
      case 6: spew_channel_32(file,blocks,6); break;
      case 7: spew_channel_32(file,blocks,7); break;
      default: channel_error(channel);        break;
      }
    }
    break;

    case CMODE|MODE_32|MODE_X: {
      switch(channel) {
      case 0: cspew_channel_32(file,blocks,0); break;
      case 1: cspew_channel_32(file,blocks,1); break;
      case 2: cspew_channel_32(file,blocks,2); break;
      case 3: cspew_channel_32(file,blocks,3); break;
      case 4: cspew_channel_32(file,blocks,4); break;
      case 5: cspew_channel_32(file,blocks,5); break;
      case 6: cspew_channel_32(file,blocks,6); break;
      case 7: cspew_channel_32(file,blocks,7); break;
      default: channel_error(channel);         break;
      }
    }
    break;

    default:
      internal_error("unhandled mode", mode);
    }

    if (file != stdout) {
      fflush(file);
      fclose(file);
    }

    return 0;
  }
  return -1;
}

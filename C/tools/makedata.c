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

#define VPRNG_IMPLEMENTATION
#include "vprng.h"

#define HEADER     "\033[95m"
#define OKBLUE     "\033[94m"
#define OKCYAN     "\033[96m"
#define OKGREEN    "\033[92m"
#define WARNING    "\033[93m"
#define FAIL       "\033[91m"
#define ENDC       "\033[0m"
#define BOLD       "\033[1m"

void test_banner(char* str)
{
  fprintf(stderr, BOLD "%s" ENDC " (%s)\n", str, VPRNG_VERSION_STR);
}

void test_banner_2(char* str, uint32_t i)
{
  fprintf(stderr, BOLD "%s %u" ENDC " (%s)\n", str,i, VPRNG_VERSION_STR);
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


// number of 256 bit chucks to produce at a time
#define BUFFER_LEN 1024

u32x8_t buffer[BUFFER_LEN];

void spew_all(FILE* file, uint64_t n)
{
  test_banner("vprng");

  vprng_t prng;
  size_t  t;

  vprng_init(&prng);

  while(--n) {
    vprng_block_fill_u32(BUFFER_LEN, buffer, &prng);
    t = fwrite(buffer, 1, BUFFER_LEN, file);
    if (t == BUFFER_LEN) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

void cspew_all(FILE* file, uint64_t n)
{
  test_banner("cvprng");

  cvprng_t prng;
  size_t   t;

  cvprng_init(&prng);

  while(--n) {
    cvprng_block_fill_u32(BUFFER_LEN, buffer, &prng);
    t = fwrite(buffer, 1, BUFFER_LEN, file);
    if (t == BUFFER_LEN) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}


static inline void spew_channel_32(FILE* file, uint64_t n, uint32_t c)
{
  test_banner_2("vprng 32-bit lane:", c);

  uint32_t data[BUFFER_LEN];
  vprng_t prng;
  size_t  t;

  vprng_init(&prng);

  while(--n) {
    for(uint32_t i=0; i<BUFFER_LEN; i++) {
      u32x8_t r = vprng_u32x8(&prng);
      data[i] = r[c];
    }
    
    t = fwrite(data, 1, BUFFER_LEN, file);
    if (t == BUFFER_LEN) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

static inline void cspew_channel_32(FILE* file, uint64_t n, uint32_t c)
{
  test_banner_2("cvprng 32-bit lane:", c);

  uint32_t data[BUFFER_LEN];
  cvprng_t prng;
  size_t   t;

  cvprng_init(&prng);

  while(--n) {
    for(uint32_t i=0; i<BUFFER_LEN; i++) {
      u32x8_t r = cvprng_u32x8(&prng);
      data[i] = r[c];
    }
    
    t = fwrite(data, 1, BUFFER_LEN, file);
    if (t == BUFFER_LEN) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

static inline void spew_channel_64(FILE* file, uint64_t n, uint32_t c)
{
  test_banner_2("vprng 64-bit lane:", c);

  uint64_t data[BUFFER_LEN];
  vprng_t  prng;
  size_t   t;

  vprng_init(&prng);

  while(--n) {
    for(uint32_t i=0; i<BUFFER_LEN; i++) {
      u64x4_t r = vprng_u64x4(&prng);
      data[i] = r[c];
    }
    
    t = fwrite(data, 1, BUFFER_LEN, file);
    if (t == BUFFER_LEN) continue;
    
    fprintf(stderr, "oh no!");
    break;
  }
}

static inline void cspew_channel_64(FILE* file, uint64_t n, uint32_t c)
{
  test_banner_2("cvprng 64-bit lane:", c);

  uint64_t data[BUFFER_LEN];
  cvprng_t prng;
  size_t   t;

  cvprng_init(&prng);

  while(--n) {
    for(uint32_t i=0; i<BUFFER_LEN; i++) {
      u64x4_t r = cvprng_u64x4(&prng);
      data[i] = r[c];
    }
    
    t = fwrite(data, 1, BUFFER_LEN, file);
    if (t == BUFFER_LEN) continue;
    
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
	 "  --id=N       vprng_id_set(N)\n"
	 "  --cvprng     2 state version\n"
	 "  --channel=N  only channel 'N' output\n"
	 "  --blocks=N   produce N blocks of %u bytes\n"
	 "  --help        \n"
	 "\n"
	 "\n", 32*BUFFER_LEN);

  exit(0);
}

int main(int argc, char** argv)
{
  uint64_t global_id    = 1;
  uint32_t mode         = 0;
  uint32_t blocks       = 0;
  uint32_t param_errors = 0;
  FILE *   file         = stdout;
  
  static struct option long_options[] = {
    {"32",         no_argument,       0, 'w'},
    {"id",         required_argument, 0, 'g'},
    {"cvprng",     no_argument,       0, 'x'},
    {"channel",    required_argument, 0, 'c'},
    {"blocks",     required_argument, 0, 'b'},
    {"help",       no_argument,       0, '?'}, 
    {0,            0,                 0,  0 }
  };

  uint32_t channel = 0;
  
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

      case 'b':
	blocks = (uint32_t)parse_u64(optarg);
	break;

      case 'c':
	channel = (uint32_t)parse_u64(optarg);
	mode   |= MODE_X;
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
    vprng_id_set(global_id);
    
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

// Marc B. Reynolds, 2022-2025
// Public Domain under http://unlicense.org, see link for details.

// hacked and reduced version: https://github.com/Marc-B-Reynolds/mini_driver_testu01
// specifically for local testing. Add support for multiple compile time configs? humm..


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
#include <inttypes.h>

#include "util.h"
#include "unif01.h"
#include "swrite.h"
#include "bbattery.h"

#define VPRNG_IMPLEMENTATION
#ifndef VPRNG_INCLUDE
#include "vprng.h"
#else
#include VPRNG_INCLUDE
#endif

#include <float.h>

#if !defined(_MSC_VER)
#define UNUSED __attribute__((unused))
#else
#define UNUSED 
#endif

#define LENGTHOF(X) (sizeof(X)/sizeof(X[0]))

//*****************************************************************************
// all ugly temp hacks

// cumulative per stat
uint16_t total_warn[201]  = { 0 };
uint16_t total_error[201] = { 0 };
double   total_peak[201]  = { 0 };

#define ALPHA 0.1
#define BETA 0.00000001
#define ITERATION_LIMIT 1
#define BREAK (ITERATION_LIMIT+1)

void DetectIteration (double pvalue, long *size, int *i)
{
  if (pvalue < BETA || pvalue > 1.0-BETA)
    (*i) = BREAK;
  else if (pvalue < ALPHA || pvalue > 1.0-ALPHA){
    (*size) = 2 * (*size);
    (*i)++;
  }
  else
    (*i) = BREAK;
}

#define RUN_TEST(TEST)                \
 while (i < ITERATION_LIMIT) {        \
   TEST;                              \
   DetectIteration (res->pVal2[gofw_Mean], &size, &i);  \
 }

long get_file_size(const char* filename)
{
  FILE* file = fopen(filename, "rb");
  
  if (file) {
    if (fseek(file, 0, SEEK_END) == 0) {
      long size = ftell(file);
      
      if (size != -1) {
	fclose(file);
	return size;
      }
      printf("error: ftell\n");
    }
    else printf("error: fseek\n");
    
    fclose(file);
  }
  else printf("error: file '%s' not found\n", filename);

   exit(-1);
}

uint64_t parse_u64(char* str)
{
  char*    end;
  uint64_t val = strtoul(str, &end, 0);

  return val;
}

//*****************************************************************************
// copy-pasta-hack

#define HEADER     "\033[95m"
#define OKBLUE     "\033[94m"
#define OKCYAN     "\033[96m"
#define OKGREEN    "\033[92m"
#define WARNING    "\033[93m"
#define FAIL       "\033[91m"
#define ENDC       "\033[0m"
#define BOLD       "\033[1m"
#define UNDERLINE  "\033[4m"

typedef struct {
  char* r;           // right
  char* i;           // interior
  char* d;           // divider
  char* l;           // left
} mini_report_table_style_set_t;


typedef struct {
  mini_report_table_style_set_t t;  // header top
  mini_report_table_style_set_t i;
  mini_report_table_style_set_t b;  //
  char* div;
} mini_report_table_style_t;

// terminal dump styles
mini_report_table_style_t mini_report_table_style_def =
  {
    .t = {.r="┌", .i="─", .d="┬", .l="┐"},
    .i = {.r="├", .i="─", .d="┼", .l="┤"},
    .b = {.r="└", .i="─", .d="┴", .l="┘"},
    .div = "│"
  };

mini_report_table_style_t mini_report_table_style_ascii =
  {
    .t = {.r="+", .i="-", .d="+", .l="+"},
    .i = {.r="+", .i="-", .d="+", .l="|"},
    .b = {.r="+", .i="-", .d="+", .l="+"},
    .div = "|"
  };


#define MINI_REPORT_TABLE_MAX_COL 16

typedef enum {
  mini_report_justify_right,  
  mini_report_justify_left,  
  mini_report_justify_center,  
} mini_report_justify_t;

typedef struct {
  uint32_t flags;
  uint8_t  width;   // total character width
  uint8_t  e_w;     // 
  uint8_t  e_p;     // 
  uint8_t  x2;      // 
  char*    text;    // header text
} mini_report_table_col_t;

typedef struct {
  mini_report_table_style_t* style;
  uint32_t flags;
  uint8_t  num_col;
  uint8_t  left_pad;
  uint8_t  x1;
  uint8_t  x2;
  mini_report_table_col_t col[MINI_REPORT_TABLE_MAX_COL];
} mini_report_table_t;


// super inefficient but who cares
void mini_report_char_rep(FILE* file, char* c, uint32_t n)
{
  while(n--) { fprintf(file,"%s",c); }
}


void mini_report_table_init(mini_report_table_t* table, uint32_t count, ...)
{
  va_list args;
  
  va_start(args, count);
  
  for (uint32_t i=0; i<count; i++) {
    char* text = va_arg(args, char*);
    table->col[i].text  = text;
    table->col[i].width = (uint8_t)strlen(text);
    table->col[i].e_w   = table->col[i].width;
    table->col[i].e_p   = 0;
  }

  table->num_col = (uint8_t)count;
  
  va_end(args);
}

void mini_report_set_col_width(mini_report_table_t* table, uint32_t col, uint32_t w, mini_report_justify_t j)
{
  if (col < table->num_col) {
    uint32_t hlen = (uint32_t)strlen(table->col[col].text);
    if (w > hlen) {
      uint32_t d = w-hlen;
      
      table->col[col].width = (uint8_t)w;

      switch(j) {
      case mini_report_justify_right:
        table->col[col].e_w   = (uint8_t)w;
        table->col[col].e_p   = 0;
	break;

      case mini_report_justify_left:
        table->col[col].e_w   = (uint8_t)(hlen);
        table->col[col].e_p   = (uint8_t)(w-hlen);
	break;

      case mini_report_justify_center:
      default:
	table->col[col].e_w   = (uint8_t)(hlen+(d>>1));
	table->col[col].e_p   = (uint8_t)(w-table->col[col].e_w);
	break;
      }
    }
  }
}


void mini_report_table_div(FILE* file, mini_report_table_t* table, mini_report_table_style_set_t* set)
{
  fprintf(file, "%s", set->r);

  mini_report_char_rep(file, set->i, table->col[0].width);

  for(uint32_t c=1; c<table->num_col; c++) {
    fprintf(file, "%s", set->d);
    mini_report_char_rep(file, set->i,table->col[c].width);
  }
  
  fprintf(file, "%s", set->l);
}

void mini_report_table_header(FILE* file, mini_report_table_t* table)
{
  mini_report_table_style_t* style = table->style;

  // top of header
  mini_report_table_div(file, table, &style->t); printf("\n");

  // each of the header entries
  for(uint32_t i=0; i<table->num_col; i++) {
    fprintf(file, "%s" BOLD "%*s" ENDC "%*s",
	    style->div,
	    table->col[i].e_w, table->col[i].text,
	    table->col[i].e_p, ""
	    );
  }
  fprintf(file, "%s\n", style->div);

  // bottom of header
  mini_report_table_div(file, table, &style->i); printf("\n");
}

void mini_report_table_end(FILE* file, mini_report_table_t* table)
{
  mini_report_table_style_t* style = table->style;

  mini_report_table_div(file, table, &style->b); printf("\n");
}

//*****************************************************************************

enum { run_alphabit, run_block, run_rabbit, run_smallcrush, run_crush };

// only 'name' is useful ATM.
typedef struct {
  char*    name;
  uint8_t  num_tests;
  uint8_t  foo;
  uint16_t num_statistics; 
} battery_info_t;

// alphabit and rabbit # of statistics depend on length of data fed:
//   alphabit on [16,17] ?? 
//   rabbit   on [32,38] ??
// would need to go look at the source again.

battery_info_t battery_info[] =
{
  [run_alphabit]   = {.name="Alphabit",       .num_tests= 9, .num_statistics=16},
  [run_block]      = {.name="Block Alphabit", .num_tests= 9, .num_statistics=16},
  [run_rabbit]     = {.name="Rabbit",         .num_tests=26, .num_statistics=32},
  [run_smallcrush] = {.name="SmallCrush",     .num_tests=10, .num_statistics=15},
  [run_crush]      = {.name="Crush",          .num_tests=96, .num_statistics=144},
};

enum { sample_lo, sample_hi, sample_rev      };

typedef struct {
  char*    name;
} sample_info_t;

battery_info_t sample_info[] =
{
  [sample_lo]  = {.name="lower 32-bits" },
  [sample_hi]  = {.name="high 32-bits" },
  [sample_rev] = {.name="bitreverse & truncated to 32-bits" }
};


// a bunch of globals because this is one-shot program. And it
// was grown like a fungus over time.
uint32_t battery = run_alphabit;
uint32_t sample  = sample_lo;
uint32_t trials  = 20;
double   battery_bits = 32.0*1000.0;
char*    filename = NULL;

uint32_t trial_num = 0;
uint32_t statistic_count  = 0;
uint32_t note_count       = 0;
uint32_t suspicious_count = 0;
uint32_t failure_count    = 0;
bool     first_reported   = false;

bool     pvalue_trim    = true;
double   pvalue_report  = 0.01;
double   pvalue_suspect = 0.001;
double   pvalue_fail    = 0x1.0p-40;

int      real_stdout;
int      null_stdout;

//*****************************************************************************
// 

mini_report_table_t table = { .style = &mini_report_table_style_def, .num_col=7 };

void print_pvalue(FILE* file, double p)
{
  double t      = fmin(p, 1.0-p);
  char*  prefix = (t > pvalue_fail) ? ""WARNING : ""FAIL;
  char*  suffix = ""ENDC;

  prefix = (t > pvalue_suspect) ? "" : prefix;
  suffix = (t > pvalue_suspect) ? "" : suffix;

  fprintf(file,"%s%*.*f%s", prefix,10,10,p,suffix);
}

void print_row(uint32_t stat_id)
{
  char* div = table.style->div;

  printf("%s"             // divider
	 "%*u"            // trial number
	 "%s"             // divider
	 "%*u"            // # of the statistic
	 "%s"             // divider
	 " %-*s"          // statistic and parameters (as much as TestU01 fills in)
	 "%s",            // divider

	 div, table.col[0].width,   trial_num,
	 div, table.col[1].width,   stat_id,
	 div, table.col[2].width-1, bbattery_TestNames[stat_id],
	 div
	 );

  // the p-value, closing divider and newline
  print_pvalue(stdout, bbattery_pVal[stat_id]);

  // mark the suspect test in rabbit if we're reporting it
  if (battery == run_rabbit && stat_id == 0)
    printf("%s ← suspect test (see docs)\n", div);
  else
    printf("%s\n", div);
}

// per trial report
void report(void)
{
  uint32_t e = (uint32_t)bbattery_NTests;

  statistic_count += e;
  
  for(uint32_t i=0; i<e; i++) {
    double pvalue = bbattery_pVal[i];
    double t      = fmin(pvalue,1.0-pvalue);
    bool   show   = true;

    if (t >= pvalue_report) continue;

    // track worst p-value
    if (t < total_peak[i])
      total_peak[i] = t;
    else if (pvalue_trim) show = false;

    // if first row to be displayed: start the table
    if (!first_reported) {
      printf("\n");
      if (trials > 1) printf("\n" BOLD "TRIALS:" ENDC "\n");
      mini_report_table_header(stdout, &table);
      first_reported = true;
    }

    if (t <= pvalue_suspect) {
      if (t > pvalue_fail) {
	suspicious_count++;
	total_warn[i]++;
      }
      else {
	failure_count++;
	total_error[i]++;
      }
    }
    
    if (show) print_row(i);
  }
}

// cumulative report (WIP)
void report_final(void)
{
  uint32_t e   = (uint32_t)bbattery_NTests;
  char*    div = table.style->div;

  printf("\n" BOLD "TOTALS:" ENDC "\n");
  
  // modify the existing table def since we're done
  mini_report_table_init(&table, 5, "   ","statistic","suspicious","   fail   ", "  worst t   ");
  mini_report_set_col_width(&table, 1, 31, mini_report_justify_center);
  mini_report_table_header(stdout, &table);
  
  for(uint32_t i=0; i<e; i++) {

    // skip statistic with no questionable/fails
    if ((total_warn[i] + total_error[i]) == 0) continue;

    printf("%s"             // divider
	   "%*u"            // # of the statistic
	   "%s"             // divider
	   " %-*s"          // statistic and parameters (as much as TestU01 fills in)
	   "%s"             // divider
	   "%*u"            // suspicious count
	   "%s"             // divider
	   "%*u"            // fail count
	   "%s"             // divider
	   "%e"
	   "%s"             // divider
	   "\n",
	   
	   div, table.col[0].width,   i,
	   div, table.col[1].width-1, bbattery_TestNames[i],
	   div, table.col[2].width,   total_warn[i],
	   div, table.col[3].width,   total_error[i],
	   div, total_peak[i],
	   div
	   );
  }

  mini_report_table_end(stdout, &table);
}



//*****************************************************************************
// TestU01 interface. just globals.

uint32_t  current_id = 8;
u32x8_t   current_draw;
uint64_t  init_id  = 1;
char*     gen_name = "vprng";

vprng_t   vprng;
cvprng_t cvprng;

unif01_Gen* gen;

bool testu01out = false;

static uint64_t vnext_u32(void* UNUSED p, void* UNUSED s)
{
  if (current_id < 8)
    return current_draw[current_id++];

  current_draw = vprng_u32x8(&vprng);
  current_id   = 1;

  return current_draw[0];
}

static uint64_t cnext_u32(void* UNUSED p, void* UNUSED s)
{
  if (current_id < 8)
    return current_draw[current_id++];

  current_draw = cvprng_u32x8(&cvprng);
  current_id   = 1;

  return current_draw[0];
}

static double vnext_f64(void* UNUSED p, void* UNUSED s)
{
  uint64_t u = vnext_u32(p,s);

  u  |= vnext_u32(p,s) << 32;
  u >>= (64-53);
  
  return (double)u*0x1.0p-53;
}

static double cnext_f64(void* UNUSED p, void* UNUSED s)
{
  uint64_t u = cnext_u32(p,s);

  u  |= cnext_u32(p,s) << 32;
  u >>= (64-53);
  
  return (double)u*0x1.0p-53;
}


static void print_state(void* UNUSED s)
{
  //printf("  counter = 0x%016" PRIx64 "\n", data.counter);
}

unif01_Gen gen_v_all = {
  .name    = "whatever",
  .GetU01  = &vnext_f64,
  .GetBits = &vnext_u32,
  .Write   = &print_state
};

unif01_Gen gen_c_all = {
  .name    = "whatever",
  .GetU01  = &cnext_f64,
  .GetBits = &cnext_u32,
  .Write   = &print_state
};

unif01_Gen* gen = &gen_v_all;

void help_options(char* name)
{
  printf("Usage: %s [OPTIONS] [FILE]\n", name);
  printf("\n"
	 "\n Batteries:\n" 
	 "  --alphabit[=LEN]     (default)\n"
	 "  --block[=LEN]        block alphabit\n"
	 "  --rabbit[=LEN]       \n"
	 "  --smallcrush         smallcrush file input requires textfile of\n"
	 "                       doubles on [0,1). I've never tried it.\n"
	 "  --crush              crush can't be run on a file\n"
	 "\n p-value limits:     thresholds to display statistic results\n"
	 "  --pshow=[VALUE]      display              (disabled by default)\n" 
	 "  --psus=[VALUE]       report as suspicious (default = 0.001)\n" 
	 "  --pfail=[VALUE]      report as fail       (default = 2^(-40))\n" 
	 "\n TestU01 Output:     any of these disable this program's output\n" 
	 "                       and uses TestU01's internal reporting instead\n" 
	 "  --short                summaries per trial only\n"
	 "  --verbose            \n"
	 "  --collectors         \n"
	 "  --classes            \n"
	 "  --counters           \n"
	 "\n Sampling            only used for non-file runs\n"
	 "  --cvprng             combined generator\n"
	 "  --id=VALUE           vprng_global_id_set value (default is random)\n"
	 "");

  exit(0);
}


// possible set of numbers is limited (this ugly..but fuck it)
void print_hr_num(uint64_t n)
{
  if (n >= 1024) {
    char s = 'K';
    n >>= 10;
    if (n >= 1024) { s='M'; n >>= 10; }
    if (n >= 1024) { s='G'; n >>= 10; }

    printf("%" PRIu64 "%c",n,s);
  }

  printf("%" PRIu64, n);
}


uint64_t parse_hr_num(char* s)
{
  char* end;

  uint64_t value = strtoull(s, &end, 10);

  switch(end[0]) {
    case 'G': value *= 1024;
    case 'M': value *= 1024;
    case 'K': value *= 1024;
  }

  return value;
}

char     battery_size_suffix = 0;
uint64_t battery_size;

void battery_set(uint32_t id)
{
  battery = id;

  if (optarg) {
    // alphabit & rabbit takes an optional argument
    char*    end;
    uint64_t val = strtoul(optarg, &end, 0);

    if (end[0] == 0) {
      double bits = (double)val * 64.0;

      if (bits >= 512.0)
	battery_bits = bits;
      else {
	printf("blocks=%s ignored. >= 8 required\n", optarg);
      }
    }
    else printf("warning: skipping block argument %s\n", optarg);
  }
}

void parse_options(int argc, char **argv)
{
  static struct option long_options[] = {
    {"alphabit",   optional_argument, 0, 'a'},
    {"block",      optional_argument, 0, 'b'},
    {"rabbit",     optional_argument, 0, 'r'},
    {"smallcrush", no_argument,       0, 's'},
    {"crush",      no_argument,       0, 'c'},
    {"cvprng",     no_argument,       0, 'C'},
    {"fundamental",no_argument,       0, 'f'},
    {"phi",        no_argument,       0, 'p'},
    {"id",         required_argument, 0, 'x'},
    {"increment",  required_argument, 0, 'i'},
    {"trials",     required_argument, 0, 't'},
    {"hash",       optional_argument, 0,  5 },
    
    {"short",      no_argument,       0,  0 },
    {"verbose",    no_argument,       0, 'v'},
    {"parameters", no_argument,       0,  1 },
    {"collectors", no_argument,       0,  2 },
    {"classes",    no_argument,       0,  3 },
    {"counters",   no_argument,       0,  4 },
    
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
    case 'x': init_id = parse_u64(optarg); break;
    case 't':
      {
	char*    end;
	uint64_t val = strtoul(optarg, &end, 0);

	if (c == 't') {
	  if (val != 0) trials = (uint32_t)val;
	}
	//else  data.counter = val;
      }
      break;

    case 'v':swrite_Basic      = TRUE; testu01out = true;  break;
    case 0:                            testu01out = true;  break;
    case 1:  swrite_Parameters = TRUE; testu01out = true;  break;
    case 2:  swrite_Collectors = TRUE; testu01out = true;  break;
    case 3:  swrite_Classes    = TRUE; testu01out = true;  break;
    case 4:  swrite_Counters   = TRUE; testu01out = true;  break;

    case 5:
      if (optarg) {
	printf("dead option\n");
	//bit_finalizer  = get_hash(optarg);
	break;
      }
      //print_xorshift_mul_3();
      exit(0);
      break;

    case 'C': gen = &gen_c_all; gen_name="cvprng"; break;
    case 'a': battery_set(run_alphabit);         break;
    case 'b': battery_set(run_block);            break;
    case 'r': battery_set(run_rabbit);           break;
    case 's': battery_set(run_smallcrush);       break;
    case 'c': battery_set(run_crush);            break;
    case 'p': break;
    case '?': help_options(argv[0]);             break;
      
    default:
      printf("internal error: what option? %c (%u)\n", c,c);
    }
  }

  if (optind == argc) return;

  // get filename and silently ignore anything past it
  filename     = argv[optind];
  battery_bits = (double)get_file_size(filename) * 8.0;

  if (battery_bits < 4096.0) {
    printf("error: datafile too small\n");
    exit(-1);
  }
}

void pre_trial(void)
{
  if (!testu01out) dup2(null_stdout, STDOUT_FILENO);
}

void post_trial(void)
{
  dup2(real_stdout, STDOUT_FILENO);
  if (!testu01out) report();
}

int main(int argc, char** argv)
{
  // default to results only
  swrite_Basic = FALSE;

  parse_options(argc, argv);

  vprng_global_id_set(init_id);
  vprng_init(&vprng);
  cvprng_init(&cvprng);
  vprng_u32x8(&vprng);
  cvprng_u32x8(&cvprng);
  
  // hack-horrific to prevent default TestU01 reporting
  real_stdout = dup(STDOUT_FILENO);
  null_stdout = open("/dev/null", O_WRONLY);
  
  // setup per trial table
  mini_report_table_init(&table, 4, "trial","   ","statistic","p-value");
  mini_report_set_col_width(&table, 2, 31, mini_report_justify_center);
  mini_report_set_col_width(&table, 3, 12, mini_report_justify_center);

  // spew some info about the tests we're performing
  printf("battery:    " BOLD "%s" ENDC "\n", battery_info[battery].name);

  for (uint32_t i=0; i<LENGTHOF(total_peak); i++) {
    total_peak[i] = 1.0;
  }
  
  if (filename) {
    printf("%s : %.0f bits\n", filename, battery_bits);
  }
  else {
    printf("source:     %s (" VPRNG_NAME " version %s)\n", gen_name, VPRNG_VERSION_STR);
    printf("vprng_init: 0x%016lx\n", init_id);
    printf("sample:     %s\n", sample_info[0].name);
    printf("trials:     %u\n", trials);

    switch(battery) {
    case run_rabbit:
    case run_alphabit:
    case run_block:
      printf("-------     %f\n", battery_bits);
      break;
    default:
      break;
    }
  }
  
  // file based or internal computation  
  if (filename) {
    if (!testu01out) dup2(null_stdout, STDOUT_FILENO);
    trials = 1;

    switch(battery) {
    case run_alphabit:   bbattery_AlphabitFile(filename, battery_bits);      break;
    case run_block:      bbattery_BlockAlphabitFile(filename, battery_bits); break;

    case run_rabbit:
      battery_bits = 0.5*battery_bits;
      fprintf(stderr, WARNING "warning" ENDC ": halving size because rabbit uses more than specified and barfs\n");
      bbattery_RabbitFile(filename,   battery_bits);
      break;

    case run_smallcrush:
      fprintf(stderr, WARNING "warning" ENDC ": SmallCrush expects a text file of floats. Don't ask me.\n");
      bbattery_SmallCrushFile(filename);
      break;

    case run_crush:
      fprintf(stderr, FAIL "error:" ENDC " Crush doesn't have a file interface\n");
      break;

    default:
      fprintf(stderr, FAIL "internal error:" ENDC " what battery??\n");
      exit(-1);
      break;
    }

    dup2(real_stdout, STDOUT_FILENO);
    if (!testu01out) report();
  }
  else {
    switch(battery) {
    case run_rabbit:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_Rabbit(gen, battery_bits);
	post_trial();
      }
      break;
      
    case run_alphabit:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_Alphabit(gen, battery_bits, 0, 32);
	post_trial();
      }
      break;
      
    case run_block:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_BlockAlphabit(gen, battery_bits, 0, 32);
	post_trial();
      }
      break;
      
    case run_smallcrush:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_SmallCrush(gen);
	post_trial();
      }
      break;
      
    case run_crush:
      for(; trial_num<trials; trial_num++) {
	pre_trial();
	bbattery_Crush(gen);
	post_trial();
      }
      break;

    default:
      printf("internal error: what battery??\n");
      exit(-1);
      break;
    }
  }

  // local multi trial summary information is gathered at per-trial reporting time.
  // can't be bothered to break it out. looking that the testu01 output means your
  // probably looking for some other information anyway.

  if (first_reported) mini_report_table_end(stdout, &table);

  if (!testu01out) {
    if ((suspicious_count+failure_count)==0) {
      printf("result:  " BOLD OKGREEN "passed all" ENDC " %u statistics\n", statistic_count);
    }
    else {
      printf("  statistics:   %10u\n", statistic_count);
      printf("    suspicious: %10u\n", suspicious_count);
      printf("    failed:     %10u\n", failure_count);
      
      if (trials > 1) {
	report_final();
      }
    }
  }
  
  return 0;
}


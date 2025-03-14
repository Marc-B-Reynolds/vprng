#pragma once

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

// 2-term
static inline uint64_t xorshift2(uint64_t s) { s ^= s << 7; s ^= s >> 9; return s; }

static inline uint64_t xorshift_x1(uint64_t x, uint32_t a, uint32_t b, uint32_t c)
{
  x ^= x << a;
  x ^= x >> b;
  x ^= x << c;
  return x;
}

static inline uint64_t xorshift_x2(uint64_t x, uint32_t a, uint32_t b, uint32_t c)
{
  x ^= x << c;
  x ^= x >> b;
  x ^= x << a;
  return x;
}

static inline uint64_t xorshift_x5(uint64_t x, uint32_t a, uint32_t b, uint32_t c)
{
  x ^= x << b;
  x ^= x << c;
  x ^= x >> a;
  return x;
}

typedef struct {
  uint64_t (*m)(uint64_t, uint32_t, uint32_t, uint32_t);
  uint32_t k[3];
} xorshift_def_t;

xorshift_def_t xorshift_def[] =
{
  {.m=xorshift_x2, .k={ 5,15,27}}, // 2
  {.m=xorshift_x5, .k={10, 7,33}}, // 2
  {.m=xorshift_x1, .k={10, 7,33}}, // 3
  {.m=xorshift_x5, .k={ 6,23,27}}, // 3
  {.m=xorshift_x2, .k={15,17,27}}, // 3
  {.m=xorshift_x2, .k={19,43,27}}, // 3
  {.m=xorshift_x1, .k={23,13,38}}, // 3
  {.m=xorshift_x2, .k={21, 9,29}}, // 3
  {.m=xorshift_x1, .k={23,15,25}}, // 3
  {.m=xorshift_x1, .k={11, 5,32}}, // 3
  {.m=xorshift_x2, .k={11, 5,32}}, // 3
};


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

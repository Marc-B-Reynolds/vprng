
#define VPRNG_IMPLEMENTATION
#include "vprng.h"


#include <stdio.h>

int main(void)
{
  vprng_t vprng0;
  vprng_t vprng1;
  
  // we have a sequence of random number generators this chooses where
  // we are in that sequence. should be called at application start up
  vprng_id_set(1);

  vprng_init(&vprng0);
  printf("%lu\n", vprng_id_get());
  
  vprng_init(&vprng1);
  printf("%lu\n", vprng_id_get());
  
  return 0;
}

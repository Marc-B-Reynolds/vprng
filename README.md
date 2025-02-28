# vprng

fast portable 256-bit SIMD pseudo random number generator.

Required compiler features:
* C11 () 
* GCC `vector_size` attribute

Required SIMD hardware operations (for performance):
* 64-bit addition
* 64-bit shift by constant (same per lane)
* 32-bit product (low 32-bit result)


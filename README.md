# vprng

fast portable 256-bit SIMD pseudo random number generators.

!!! TIP
    This is an alpha version
	
Implements two generators:


Required compiler features:
* C11 for standard atomic operations
* GCC `vector_size` attribute

Required SIMD hardware operations (for performance):
* 64-bit addition
* 64-bit shift by constant (same per lane)
* 32-bit product (low 32-bit result)

Example variants can *up the ante* on hardware operations.

For speed it requires one more logical operation (an xorshift) vs.
*SplitMix64* (SIMD instead of scalar ops) 


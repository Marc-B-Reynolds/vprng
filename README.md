# vprng

fast *portable* 256-bit SIMD pseudo random number generators.

**This is an alpha version**

Implements two types generators. A single state update and a combined two state update both
with a bit finalizing mixer. There are various example configutations beyond the baseline
default provided.

Required compiler features:
* C11 for standard atomic operations
* GCC `vector_size` attribute

Required SIMD hardware operations (for performance):
* 64-bit addition
* 64-bit shift by constant (same per lane)
* 32-bit product (low 32-bit result)

Example variants can *up the ante* on hardware operations and/or make performance/quality tradeoffs.

The *baseline* version is a single header file [vprng.h](C/vprng.h)

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

The *baseline* version is a single header file `vprng.h` 


** f64 noted the current baseline version fails PractRand very quicky:**
with `-tf2` option: *suspicious* p-values at merely 16MB and straight fail at 32MB. Sadface.
Investigation indicates a large reduction in max bias of the finalizer is needed to improve
this. Since the finalizer falls on the result path I'm considering possible structural
changes. (the trick is limiting to 32-bit products and common SIMD ops for max arch support)

(STRIPPED COMMENTS UNTIL I REVIST FINALIZER AND STRUCTURE)


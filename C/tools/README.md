
Development related tools.  <small>all copy-pasta hacks</small>
======================================

## makedata


`./makedata | RNG_test stdin64 -tlmin 1MB -tlmax 512GB -seed 1 | tee data/practrand_vprng_all_1.txt`

## testu01


## Other tools


* `self_check`: minimal internal 
* `timing`:  garbage benchmarking that overly aggressively looks for peak throughput values. intended as dev aid only.
* `xorshift`: builds initial state values for `cvprng`. Requires [M4RI](https://github.com/malb/m4ri) installed. 


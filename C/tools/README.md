
Development related tools.  <small>all copy-pasta hacks</small>
======================================

The `Makefile` is utter garbage and any tool that has external requirements must be explictly list on the command-line.

# practrand_run.sh

This is a trival `bash` script for feeding output of `makedata` (below) to the `PractRand` executable `RNG_test` and sending the output to a file.


# makedata

`makedata` allows creating a `vprng` or `cvprng` and sending it's output to either a file or to `stdout`. It only supports one configuration of generators per executable which can be specified via a macro. example: `-DVPRNG_INCLUDE=\"vpcg.h\"` to use the `vpcg.h` wrapper. 

`./makedata | RNG_test stdin64 -tlmin 1MB -tlmax 512GB -seed 1 | tee data/practrand_vprng_all_1.txt`

# vprng_testu01

This is a minimal front-end for [`TestU01`](https://github.com/umontreal-simul/TestU01-2009/) which is a hack of (this)[https://github.com/Marc-B-Reynolds/mini_driver_testu01]. This is probably broken ATM.

# Other tools

* `self_check`: minimal internal checks
* `timing`:  garbage benchmarking that overly aggressively looks for peak throughput values. intended as dev aid only.
* `xorshift`: builds initial state values for `cvprng`. Requires [M4RI](https://github.com/malb/m4ri) installed.


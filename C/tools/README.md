
Development related tools.  <small>all copy-pasta hacks</small>
======================================

The `Makefile` is utter garbage and any tool that has external requirements must be explictly listed on the command-line. Try:

    make help
	
and good luck.


# practrand_run.sh

This is a trival `bash` script for feeding output of `makedata` (below) to the `PractRand` executable `RNG_test` and sending the output to a file. Try:

    ./practrand_run.sh --help 


    data/practrand_{variant}{mods}_{channels}_{arg}.txt


# FWIW

The various executables need to be compiled per configuration. (`vprng.h`, `vpcg.h`, etc)

## makedata

`makedata` allows creating a `vprng` or `cvprng` instance and sending it's output to either a file or to `stdout`. 

`./makedata | RNG_test stdin64 -tlmin 1MB -tlmax 512GB`


## vprng_testu01

This is a minimal front-end for [`TestU01`](https://github.com/umontreal-simul/TestU01-2009/) which is a hack of (this)[https://github.com/Marc-B-Reynolds/mini_driver_testu01]. This is probably broken ATM. It's certainly not all wired together enough to be useful.

## Other tools (generator specific)

* `self_check`: minimal internal checks (mostly useless ATM)
* `timing`:     garbage benchmarking that overly aggressively looks for peak throughput values. intended as dev aid only.

## Other tools (not generator specific)
* `xorshift`:   builds initial state values for `cvprng`. Requires [M4RI](https://github.com/malb/m4ri) installed.


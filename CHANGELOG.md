Bullet pointy stuff


-----------------------------------------------
<small>0.0.2</small>

* greatly improved the default bit-mixer
* added variants:
  * 12AES variant `vprng_aes.h`
  * 32-bit PCG `vpcg32.h`
  * 64-bit PCG `vpcg.h`
  * 64-bit SplitMix `vsplitmix.h`

-----------------------------------------------
<small>0.0.1</small>

First *alpha* release. The bit finalizer hasn't been examined and it was built on *vibes*

* compile time generator selection
* improved coverage of mutiple generators in `makedata`
* GitHub user fp64 note a bonedhead bug in `makedata` which caused only a fraction of the random sequences being sent to output.

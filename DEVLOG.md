Side line notes: mostly self notes to refresh my memory, food for thought
on what seem to be major points internally.


-----------------------------------------------
<small>0.0.2</small>

* Hand tweaked the finalizers. Don't like the structure of the shifts
and mixing performed in the low 32-bits isn't properly propagated 
back up the the high 32-bits. But anyway..currently a huge
improvement for the original (but a bit slower)

     |bias| = 0.00089735 � 0.00081506 : max = 0.009876
     |bias| = 0.00101875 � 0.00115915 : max = 0.012527
     |bias| = 0.00101763 � 0.00106683 : max = 0.011875
     |bias| = 0.00085375 � 0.00079878 : max = 0.017200
	 
* Initial AES version (again) has hacked mixer. Need to think about
  bit propagation, periods of base sequence and make a measuring tool.
  It's performing quite well as is and minimal testing seems to
  indicate it's pretty good as is.

* Changed the global constant generator. Need to revisit that
  as well. Doesn't matter for stronger hash version but could
  help the weaker ones quite a bit. A MLCG constant instead
  instead of trying to

-----------------------------------------------
<small>0.0.1</small>

The initial checked in version used 8 unique 32-bit finalizers (`vprng_mix`) which
were tossed together by adapting well performing proper 32-bit finalizers
and modifing the shift parts to 64-bit. It was passing PractRand up to 512GB using
the default options. The GitHub user `fp64` the pointed out that using the
`-tf 2` option caused it to consistently fail at merely 8MB. Closer examination
pointed to the finalizer as being the issue.

original version SAC bias (per 64-bit block):

    |bias| = 0.01108408 � 0.03786583 : max = 0.523777
    |bias| = 0.01173061 � 0.03889093 : max = 0.524384
    |bias| = 0.00962076 � 0.03821139 : max = 0.572851
    |bias| = 0.00995480 � 0.03496292 : max = 0.500401
	
and compared to MIX14 (which is used by SplitMix64 and others) and MIX03 (which
I measure as being better at Weyl sequence inputs)

    |bias| = 0.00078181 � 0.00059679 : max = 0.003954 (mix14)
    |bias| = 0.00077872 � 0.00059169 : max = 0.003340 (mix03)


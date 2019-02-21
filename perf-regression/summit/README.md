These are example scripts for running Mochi regression tests on Summit 
(or summitdev).

Notes:
* 'module swap xl gcc/6.4.0' first (any gcc compiler should be fine, but that 6.4. is known to work without gcc install prefix problems evident on some of the other gcc builds on summitdev)
* do not build your own verbs (rdma-core) libraries.  They will interfere with dlopen of spectrum-mpi components.
* PMDK will not work on summit, at least as of February 2019.  It does not support the power architecture.
* As of 2019-02-20, must hack Argobots a little by commenting out line 93 of malloc.c, see https://github.com/pmodels/argobots/issues/93.  All Margo tests complete successfully with this hack in place.  Will revisit once proper fix available.

These are example scripts for running Mochi regression tests on Summit 
(or summitdev).

Notes:
* 'module swap xl gcc/6.4.0' first (any gcc compiler should be fine, but that 6.4. is known to work without gcc install prefix problems evident on some of the other gcc builds on summitdev)
* do not build your own verbs (rdma-core) libraries.  They will interfere with
  dlopen of spectrum-mpi components.  If you get a message about "No active IB
  device ports detected", then the rdma-core library you built is interfering
  with the one specturm-mpi wants to load.
* PMDK will not work on summit, at least as of February 2019.  It does not support the power architecture.
* As of 2019-02-20, must hack Argobots a little by commenting out line 93 of malloc.c, see https://github.com/pmodels/argobots/issues/93.  All Margo tests complete successfully with this hack in place.  Will revisit once proper fix available.
* libfabric needs both `verbs` and `rxm` providers.  If you configure with only `verbs`, you will get an error like this:  `fi_getinfo() failed, rc: -61(No data available)`
* To run the server/provider in one process and client in another, you must disable 'cgroups':  submit your job with `#BSUB -step_cgroup n`
* If you want the server/provider to run on one node and client on another, request multiple nodes with 'bsub' and run with 'jsrun -n 1 -r 1 -g ALL_GPUS'.  You can background one jsrun to start the server and then run another jsrun to start the client

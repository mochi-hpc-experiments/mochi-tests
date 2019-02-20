These are example scripts for running Mochi regression tests on Summit 
(or summitdev).

Notes:
* 'module swap xl gcc/6.4.0' first (any gcc compiler should be fine, but that 6.4. is known to work without gcc install prefix problems evident on some of the other gcc builds on summitdev)
* do not build your own verbs (rdma-core) libraries.  They will interfere with dlopen of spectrum-mpi components.
* PMDK will not work on summit, at least as of February 2019.  It does not support the power architecture.

Status (margo-p2p-latency):

Process 0 of 2 is on summitdev-r0c0n12
# <op> <iterations> <size> <min> <q1> <med> <avg> <q3> <max>
noop	100000	0	0.000011921	0.000012875	0.000013351	0.0000156490.000018358	0.025480509
[summitdev-r0c0n13:04037] *** Process received signal ***
[summitdev-r0c0n13:04037] Signal: Segmentation fault (11)
[summitdev-r0c0n13:04037] Signal code: Invalid permissions (2)
[summitdev-r0c0n13:04037] Failing at address: 0x10000073c318
[summitdev-r0c0n13:04037] [ 0] [0x100000050478]
[summitdev-r0c0n13:04037] [ 1] ./margo-p2p-latency[0x100029d0]
[summitdev-r0c0n13:04037] [ 2] /autofs/nccs-svm1_home1/carns/working/src/spack/opt/spack/linux-rhel7-ppc64le/gcc-6.4.0/argobots-develop-4l7rzjstalkuojewr6ruxbyrcxfw7t7i/lib/libabt.so.0(+0x9de4)[0x1000003f9de4]

Status (margo-p2p-bw):

[summitdev-r0c0n09:04448] *** Process received signal ***
[summitdev-r0c0n09:04448] Signal: Segmentation fault (11)
[summitdev-r0c0n09:04448] Signal code: Invalid permissions (2)
[summitdev-r0c0n09:04448] Failing at address: 0x10000073c318
[summitdev-r0c0n09:04448] [ 0] [0x100000050478]
[summitdev-r0c0n09:04448] [ 1] [0x0]
[summitdev-r0c0n09:04448] [ 2] /autofs/nccs-svm1_home1/carns/working/src/spack/opt/spack/linux-rhel7-ppc64le/gcc-6.4.0/argobots-develop-4l7rzjstalkuojewr6ruxbyrcxfw7t7i/lib/libabt.so.0(+0x9de4)[0x1000003f9de4]


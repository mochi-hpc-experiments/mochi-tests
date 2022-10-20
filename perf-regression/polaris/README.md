This is an example script for executing an automated regression test on the 
Polaris system at the ALCF.

To build the test application, use [Makefile.polaris](../../Makefile.polaris)

On Polaris, `mpiexec -n 2` doesn't distribute job to multiple nodes.
Thus, `mpiexec -n 1 -ppn 1` must be used for each node.
However, [gpu-margo-p2p-bw](https://github.com/HDFGroup/mochi-tests/blob/main/perf-regression/gpu-margo-p2p-bw.cu) test will fail
because the argument for `-n` must be exactly 2.

The `-x 4096` option also failed on debug queue.
Use a smaller number like `-x 1`.

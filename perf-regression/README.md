margo-p2p-latency
---------------------
margo-p2p-latency is a point to point latency benchmark.  It measures round
trip latency for a noop (i.e. as close to an empty request and response
structure as possible) RPC.

Example compile (must build with MPI support):

```
./configure <normal arguments> CC=mpicc
make 
make tests
```

Example execution (requires mpi):

```
mpiexec -n 2 tests/perf-regression/margo-p2p-latency -i 10 -n sm://
```

-i is number of iterations 
-n is transport to use

margo-p2p-bw
---------------------
margo-p2p-bw is a point to point bandwidth benchmark.  It measures Margo
(Mercury) bulk transfer operations in both PULL and PUSH mode and includes
command line arguments to control the concurrency level.

The timing and bandwidth calculation is performed by the client and includes
a round trip RPC to initiate and complete the streaming transfer.

Example compile (must build with MPI support):

```
./configure <normal arguments> CC=mpicc
make 
make tests
```

Example execution (requires mpi):

```
mpiexec -n 2 ./margo-p2p-bw -x 4096 -n sm:// -c 1 -D 10
```

-x is the transfer size per bulk operation
-n is transport to use
-c is the concurrency level
-D is the duration of the test (for each direction)

node-micro-bench
----------------------
node-microbench is a basic benchmark that measures the cost of performing
particular local operations.  It is an MPI program but is only meant to
execute on a single process.  It may optionally be compiled with Argobots
support to measure the cost of a few key Argobots primitives as well.

It takes a single argument to specify the number (in millions) of iterations
to perform of each operation to be measured:

```
> perf-regression/node-microbench -m 1
#<test case>	<m_ops>	<total s>	<m_ops/s>	<ns/op>
"fn_call_normal"	1	0.004363	229.197413	4.363051
"fn_call_inline"	1	0.003397	294.400011	3.396739
"fn_call_cross_object"	1	0.003826	261.393284	3.825653
"mpi_wtime"	1	0.055161	18.128607	55.161436
"gettimeofday"	1	0.033851	29.541277	33.850940
...
```

## abt-eventual-bench

abt-eventual-bench measures how well we can create/wait/set/destroy concurrent
eventuals. It approximates a concurrent mochi provider workload by creating N
concurrent threads, each of which (in a loop) creates detached sub-threads that
`margo_thread_sleep()` for a specified amount of time before setting an eventual
and exiting. The sleep is a stand-in for waiting on network activity.

Example, with a duration of 5 seconds, a sleep interval of 1 ms, and 16 ULTs:

```
$ ./abt-eventual-bench -d 5 -i 1 -n 16
#<num_ults>	<test_duration_sec>	<interval_msec>	<ops/s>	<cpu_time_sec>
16	5	1	15952.800000	4.255606
```

The above example shows 16 ULTs getting nearly 16,000 operations per second,
which is pretty much ideal (each can do 1000 per second if everything is
working right because there is a 1ms delay in each operation). The last column
shows CPU time. In this case a 5 second run took 4.2 seconds of user-space CPU
time, which is not expected.

These results are most interesting if you sweep across a range of ULT counts to
see how behavior changes with scale. There are scripts (in the 'tests'
subdirectory) included in this repository to do this and plot the results for
an example scenario. he first takes the eventual executable as it's argument
(so that it can account for different build directories). The second takes the
.dat file produced by the first script as it's argument and produces two plots.

### Generating the data:

```
> tests/sweep-abt-eventual-bench.sh build/perf-regression/abt-eventual-bench
writing results to sweep-abt-eventual-bench.1122789.dat
... running benchmark with 100 ULTs
... running benchmark with 200 ULTs
... running benchmark with 300 ULTs
... running benchmark with 400 ULTs
... running benchmark with 500 ULTs
... running benchmark with 600 ULTs
... running benchmark with 700 ULTs
... running benchmark with 800 ULTs
... running benchmark with 900 ULTs
... running benchmark with 1000 ULTs
results are in sweep-abt-eventual-bench.1122789.dat
```

#### A note about job schedulers

If you are running on a machine that requires you to submit jobs to compute
nodes, we typically obtain an interactive node and then pass the job launching
parts to the run script.  For example on Summit:

```
$ bsub -W 0:10 -nnodes 1 -P ${PROJECT} -Is /bin/bash
...
$ ../tests/sweep-abt-eventual-bench.sh jsrun -r1 -n1 -a1 perf-regression/abt-eventual-bench
```

### Plotting the results:

```
> tests/plot-abt-eventual-bench.sh sweep-abt-eventual-bench.1122789.dat
> ls *.png
sweep-abt-eventual-bench.1122789.dat.cpu.png
sweep-abt-eventual-bench.1122789.dat.rate.png
```

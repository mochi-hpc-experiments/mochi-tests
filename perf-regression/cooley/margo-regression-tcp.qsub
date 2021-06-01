#!/bin/bash
#COBALT -n 2
#COBALT -t 30
#COBALT --mode script
#COBALT -A radix-io
#COBALT -q ibleaf3-debug

export HOME=$SANDBOX
# note: disable registration cache for verbs provider for now; see
#       discussion in https://github.com/ofiwg/libfabric/issues/5244
export FI_MR_CACHE_MAX_COUNT=0
# use shared recv context in RXM; should improve scalability
export FI_OFI_RXM_USE_SRX=1

. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r mochi-bake

spack find --loaded

# echo "## MPI (one way, double the latency for round trip):"
# mpirun -f $COBALT_NODEFILE -n 2 ./osu_latency

echo "### NOTE: all benchmarks are using numactl to keep processes on socket 1"

host0=`head -n 1 $COBALT_NODEFILE` 
host1=`tail -n 1 $COBALT_NODEFILE` 

for xport in bmi+tcp ofi+tcp ofi+sockets; do
	sleep 1

	echo "## Margo $xport (round trip):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-latency -i 100 -n "$xport://$host0:3334" : -n 1 numactl -N 1 -m 1 ./margo-p2p-latency -i 100 -n "$xport://$host1:3334"
	echo "## Margo $xport (bw, 1MiB):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1048576 -n "$xport://$host0:3334" -c 1 -D 20 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1048576 -n "$xport://$host1:3334" -c 1 -D 20
	echo "## Margo $xport (bw, 1MiB, 8x concurrency):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1048576 -n "$xport://$host0:3334" -c 8 -D 20 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1048576 -n "$xport://$host1:3334" -c 8 -D 20
	echo "## Margo $xport (bw, 8MiB):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 8388608 -n "$xport://$host0:3334" -c 1 -D 20 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 8388608 -n "$xport://$host1:3334" -c 1 -D 20
	echo "## Margo $xport (bw, 8MiB, 8x concurrency):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 8388608 -n "$xport://$host0:3334" -c 8 -D 20 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 8388608 -n "$xport://$host1:3334" -c 8 -D 20
	echo "## Margo $xport (bw, 1MB unaligned):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1000000 -n "$xport://$host0:3334" -c 1 -D 20 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1000000 -n "$xport://$host1:3334" -c 1 -D 20
	echo "## Margo $xport (bw, 1MB unaligned, 8x concurrency):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1000000 -n "$xport://$host0:3334" -c 8 -D 20 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1000000 -n "$xport://$host1:3334" -c 8 -D 20

	sleep 1 

	echo "## Margo $xport (round trip, Hg busy spin):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-latency -i 100 -n "$xport://$host0:3334" -t 0,0 : -n 1 numactl -N 1 -m 1 ./margo-p2p-latency -i 100 -n "$xport://$host1:3334" -t 0,0
	echo "## Margo $xport (bw, 1MiB, Hg busy spin):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1048576 -n "$xport://$host0:3334" -c 1 -D 20 -t 0,0 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1048576 -n "$xport://$host1:3334" -c 1 -D 20 -t 0,0
	echo "## Margo $xport (bw, 1MiB, 8x concurrency, Hg busy spin):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1048576 -n "$xport://$host0:3334" -c 8 -D 20 -t 0,0 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1048576 -n "$xport://$host1:3334" -c 8 -D 20 -t 0,0
	echo "## Margo $xport (bw, 8MiB, Hg busy spin):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 8388608 -n "$xport://$host0:3334" -c 1 -D 20 -t 0,0 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 8388608 -n "$xport://$host1:3334" -c 1 -D 20 -t 0,0
	echo "## Margo $xport (bw, 8MiB, 8x concurrency, Hg busy spin):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 8388608 -n "$xport://$host0:3334" -c 8 -D 20 -t 0,0 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 8388608 -n "$xport://$host1:3334" -c 8 -D 20 -t 0,0
	echo "## Margo $xport (bw, 1MB unaligned, Hg busy spin):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1000000 -n "$xport://$host0:3334" -c 1 -D 20 -t 0,0 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1000000 -n "$xport://$host1:3334" -c 1 -D 20 -t 0,0
	echo "## Margo $xport (bw, 1MB unaligned, 8x concurrency, Hg busy spin):"
	mpirun -f $COBALT_NODEFILE -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1000000 -n "$xport://$host0:3334" -c 8 -D 20 -t 0,0 : -n 1 numactl -N 1 -m 1 ./margo-p2p-bw -x 1000000 -n "$xport://$host1:3334" -c 8 -D 20 -t 0,0

done


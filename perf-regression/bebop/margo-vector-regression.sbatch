#!/bin/bash
#SBATCH -N 2
#SBATCH -A startup-carns
#SBATCH --ntasks-per-node=1
#SBATCH --time=15:00
#SBATCH -p bdwall

# example of running benchmarks with spack-compiled software stack

. $SANDBOX/spack/share/spack/setup-env.sh

spack load -r mochi-bake

spack find --loaded

# make sure that MPI and libfabric can share PSM2
export PSM2_MULTI_EP=1

echo "### NOTE: all benchmarks are using numactl to keep processes on socket 0"

echo "## Margo OFI/PSM2 (vector benchmark with len 1, 512KiB xfers):"
mpirun numactl -N 0 -m 0 ./margo-p2p-vector -x 524288 -n "psm2://" -c 1 -D 20

sleep 1

echo "## Margo OFI/PSM2 (vector benchmark with len 256, 512KiB xfers):"
mpirun numactl -N 0 -m 0 ./margo-p2p-vector -x 524288 -n "psm2://" -c 1 -D 20 -v 256

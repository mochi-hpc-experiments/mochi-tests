#!/bin/bash
#SBATCH -N 2
#SBATCH -A radix-io
#SBATCH --ntasks-per-node=1
#SBATCH --time=15:00
#SBATCH -p bdwall

export HOME=$SANDBOX
. $SANDBOX/spack/share/spack/setup-env.sh
spack env activate mochi-regression
spack find -vN

# make sure that MPI and libfabric can share PSM2
export PSM2_MULTI_EP=1

echo "### NOTE: all benchmarks are using numactl to keep processes on socket 0"

echo "## Margo OFI/PSM2 (round trip, bdw):"
mpirun numactl -N 0 -m 0 ./margo-p2p-latency -i 100000 -n psm2://
echo "## Margo OFI/PSM2 (bw, 1MiB, bdw):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 1048576 -n psm2:// -c 1 -D 20
echo "## Margo OFI/PSM2 (bw, 1MiB, 8x concurrency, bdw):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 1048576 -n psm2:// -c 8 -D 20
echo "## Margo OFI/PSM2 (bw, 8MiB, bdw):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 8388608 -n psm2:// -c 1 -D 20
echo "## Margo OFI/PSM2 (bw, 8MiB, 8x concurrency, bdw):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 8388608 -n psm2:// -c 8 -D 20
echo "## Margo OFI/PSM2 (bw, 1MB unaligned, bdw):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 1000000 -n psm2:// -c 1 -D 20
echo "## Margo OFI/PSM2 (bw, 1MB unaligned, 8x concurrency, bdw):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 1000000 -n psm2:// -c 8 -D 20
# echo "## Margo OFI/PSM2 (bw, 8MiB, mmap shmfs on svr, bdw):"
# mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 8388608 -n psm2:// -c 1 -D 20 -m /dev/shm/foo
# echo "## Margo OFI/PSM2 (bw, 8MiB, mmap shmfs on svr, 8x concurrency, bdw):"
# mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 8388608 -n psm2:// -c 8 -D 20 -m /dev/shm/foo

echo "## Margo OFI/PSM2 (round trip, bdw, Hg busy spin):"
mpirun numactl -N 0 -m 0 ./margo-p2p-latency -i 100000 -n psm2:// -t 0,0
echo "## Margo OFI/PSM2 (bw, 1MiB, bdw, Hg busy spin):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 1048576 -n psm2:// -c 1 -D 20 -t 0,0
echo "## Margo OFI/PSM2 (bw, 1MiB, 8x concurrency, bdw, Hg busy spin):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 1048576 -n psm2:// -c 8 -D 20 -t 0,0
echo "## Margo OFI/PSM2 (bw, 8MiB, bdw, Hg busy spin):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 8388608 -n psm2:// -c 1 -D 20 -t 0,0
echo "## Margo OFI/PSM2 (bw, 8MiB, 8x concurrency, bdw, Hg busy spin):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 8388608 -n psm2:// -c 8 -D 20 -t 0,0
echo "## Margo OFI/PSM2 (bw, 1MB unaligned, bdw, Hg busy spin):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 1000000 -n psm2:// -c 1 -D 20 -t 0,0
echo "## Margo OFI/PSM2 (bw, 1MB unaligned, 8x concurrency, bdw, Hg busy spin):"
mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 1000000 -n psm2:// -c 8 -D 20 -t 0,0
# echo "## Margo OFI/PSM2 (bw, 8MiB, mmap shmfs on svr, bdw, Hg busy spin):"
# mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 8388608 -n psm2:// -c 1 -D 20 -t 0,0 -m /dev/shm/foo
# echo "## Margo OFI/PSM2 (bw, 8MiB, mmap shmfs on svr, 8x concurrency, bdw, Hg busy spin):"
# mpirun numactl -N 0 -m 0 ./margo-p2p-bw -x 8388608 -n psm2:// -c 8 -D 20 -t 0,0 -m /dev/shm/foo
 

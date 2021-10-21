#!/bin/bash
#SBATCH -N 1
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

echo "## PMDK (8x concurrency):"
rm -f /dev/shm/foo.dat
truncate -s 60G /dev/shm/foo.dat
pmempool create obj /dev/shm/foo.dat
./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8

echo "## PMDK (8x concurrency, 8 es):"
rm -f /dev/shm/foo.dat
truncate -s 60G /dev/shm/foo.dat
pmempool create obj /dev/shm/foo.dat
./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 -T 8

echo "## PMDK (8x concurrency, preallocated pool):"
rm -f /dev/shm/foo.dat
dd if=/dev/zero of=/dev/shm/foo.dat bs=1M count=61440
pmempool create obj /dev/shm/foo.dat
./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8

echo "## PMDK (8x concurrency, 8 es, preallocated pool):"
rm -f /dev/shm/foo.dat
dd if=/dev/zero of=/dev/shm/foo.dat bs=1M count=61440
pmempool create obj /dev/shm/foo.dat
./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 -T 8

#!/bin/bash

SANDBOX=$1

. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r ssg 
spack load -r bake
spack load -r mpich

module list

export LD_LIBRARY_PATH=$LIBRARY_PATH

echo "## PMDK (8x concurrency):"
rm -f /dev/shm/foo.dat
truncate -s 60G /dev/shm/foo.dat
pmempool create obj /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 1 ./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 

echo "## PMDK (8x concurrency, 8 es):"
rm -f /dev/shm/foo.dat
truncate -s 60G /dev/shm/foo.dat
pmempool create obj /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 1 ./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 -T 8

echo "## PMDK (8x concurrency, preallocated pool):"
rm -f /dev/shm/foo.dat
dd if=/dev/zero of=/dev/shm/foo.dat bs=1M count=61440
pmempool create obj /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 1 ./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 

echo "## PMDK (8x concurrency, 8 es, preallocated pool):"
rm -f /dev/shm/foo.dat
dd if=/dev/zero of=/dev/shm/foo.dat bs=1M count=61440
pmempool create obj /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 1 ./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 -T 8
rm -f /dev/shm/foo.dat


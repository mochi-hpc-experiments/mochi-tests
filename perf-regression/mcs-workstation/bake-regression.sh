#!/bin/bash

SANDBOX=$1

. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r ssg 
spack load -r bake
spack load -r mpich

module list

export LD_LIBRARY_PATH=$LIBRARY_PATH

echo "## Bake TCP:"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 2 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "tcp" -p /dev/shm/foo.dat -c 1 

echo "## Bake TCP (8x concurrency):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 2 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "tcp" -p /dev/shm/foo.dat -c 8 

echo "## Bake TCP (Hg busy spin):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 2 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "tcp" -p /dev/shm/foo.dat -c 1 -t 0,0

echo "## Bake TCP (8x concurrency, Hg busy spin):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 2 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "tcp" -p /dev/shm/foo.dat -c 8 -t 0,0
rm -f /dev/shm/foo.dat

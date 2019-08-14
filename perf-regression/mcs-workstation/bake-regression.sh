#!/bin/bash

SANDBOX=$1

. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r ssg 
spack load -r bake
spack load -r mpich

module list

# NOTE: rpath doesn't seem to be set correctly, and the paths we need are 
#  in LIBRARY_PATH instead of LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$LIBRARY_PATH
export PATH=$PATH:$HOME/sds-tests-install/bin
# find nodes in job.  We have to do this so that we can manually specify 
# in each mpirun so that server ranks consitently run on node where we
# set up storage space
#declare -a nodes=($(python /home/carns/bin/run_on_all_nids.py));

#echo "### NOTE: all benchmarks are using numactl to keep processes on socket 0"

echo "## Bake OFI/GNI:"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 2 bake-p2p-bw -x 16777216 -m 34359738368 -n "tcp" -p /dev/shm/foo.dat -c 1 

echo "## Bake OFI/GNI (8x concurrency):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 2 bake-p2p-bw -x 16777216 -m 34359738368 -n "tcp" -p /dev/shm/foo.dat -c 8 

echo "## Bake OFI/GNI (Hg busy spin):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 2 bake-p2p-bw -x 16777216 -m 34359738368 -n "tcp" -p /dev/shm/foo.dat -c 1 -t 0,0

echo "## Bake OFI/GNI (8x concurrency, Hg busy spin):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
ls -alh /dev/shm/foo.dat
mpirun -np 2 bake-p2p-bw -x 16777216 -m 34359738368 -n "tcp" -p /dev/shm/foo.dat -c 8 -t 0,0
rm -f /dev/shm/foo.dat

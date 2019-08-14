#!/bin/bash

SANDBOX=$1

. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r ssg 
spack load -r bake

module list

# NOTE: rpath doesn't seem to be set correctly, and the paths we need are 
#  in LIBRARY_PATH instead of LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$LIBRARY_PATH

# find nodes in job.  We have to do this so that we can manually specify 
# in each aprun so that server ranks consitently run on node where we
# set up storage space
declare -a nodes=($(python /home/carns/bin/run_on_all_nids.py));

echo "### NOTE: all benchmarks are using numactl to keep processes on socket 0"

echo "## testing launcher placement:"
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} hostname

echo "## Bake OFI/GNI:"
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} bake-mkpool -s 60G /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ls -alh /dev/shm/foo.dat
aprun -n 2 -N 1 -L ${nodes[0]},${nodes[1]} numactl -N 0 -m 0 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "ofi+gni://ipogif0:5000" -p /dev/shm/foo.dat -c 1 

echo "## Bake OFI/GNI (8x concurrency):"
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} bake-mkpool -s 60G /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ls -alh /dev/shm/foo.dat
aprun -n 2 -N 1 -L ${nodes[0]},${nodes[1]} numactl -N 0 -m 0 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "ofi+gni://ipogif0:5000" -p /dev/shm/foo.dat -c 8 

echo "## Bake OFI/GNI (Hg busy spin):"
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} bake-mkpool -s 60G /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ls -alh /dev/shm/foo.dat
aprun -n 2 -N 1 -L ${nodes[0]},${nodes[1]} numactl -N 0 -m 0 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "ofi+gni://ipogif0:5000" -p /dev/shm/foo.dat -c 1 -t 0,0

echo "## Bake OFI/GNI (8x concurrency, Hg busy spin):"
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} bake-mkpool -s 60G /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ls -alh /dev/shm/foo.dat
aprun -n 2 -N 1 -L ${nodes[0]},${nodes[1]} numactl -N 0 -m 0 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "ofi+gni://ipogif0:5000" -p /dev/shm/foo.dat -c 8 -t 0,0


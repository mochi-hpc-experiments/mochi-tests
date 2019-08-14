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

echo "## PMDK (8x concurrency):"
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} truncate -s 60G /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} pmempool create obj /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ls -alh /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 

echo "## PMDK (8x concurrency, 8 es):"
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} truncate -s 60G /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} pmempool create obj /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ls -alh /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 -T 8

echo "## PMDK (8x concurrency, preallocated pool):"
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} dd if=/dev/zero of=/dev/shm/foo.dat bs=1M count=61440
aprun -n 1 -N 1 -L ${nodes[0]} pmempool create obj /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ls -alh /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 

echo "## PMDK (8x concurrency, 8 es, preallocated pool):"
aprun -n 1 -N 1 -L ${nodes[0]} hostname
aprun -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} dd if=/dev/zero of=/dev/shm/foo.dat bs=1M count=61440
aprun -n 1 -N 1 -L ${nodes[0]} pmempool create obj /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ls -alh /dev/shm/foo.dat
aprun -n 1 -N 1 -L ${nodes[0]} ./pmdk-bw -x 16777216 -m 34359738368 -p /dev/shm/foo.dat -c 8 -T 8


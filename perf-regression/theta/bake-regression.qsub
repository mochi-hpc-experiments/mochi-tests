#!/bin/bash
#COBALT -n 2
#COBALT -t 20
#COBALT --mode script
#COBALT -A CSC250STDM12
#COBALT -q debug-cache-quad

export HOME=$SANDBOX
# XXX xalt module currently eating '-M' flag for mercury-runner...disabling for now
# module unload xalt

# necessary when using the udreg option in Mercury
export MPICH_GNI_NDREG_ENTRIES=1024

module swap PrgEnv-intel PrgEnv-gnu
module load cce

. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r mochi-ssg
spack load -r mochi-bake

spack find --loaded

# workaround for missing library paths on Cray as of 2021-03-24
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$LIBRARY_PATH"

# find nodes in job.  We have to do this so that we can manually specify 
# in each aprun so that server ranks consitently run on node where we
# set up storage space
declare -a nodes=($(python /home/carns/bin/run_on_all_nids.py));

echo "### NOTE: all benchmarks are using aprun -cc none to allow processes to run on all available cores; the default aprun settings limit processes to one core and produce poor performance because of contention between internal threads"
echo "### NOTE: all benchmarks are using numactl to keep processes on socket 0"

echo "## testing launcher placement:"
aprun -cc none -n 1 -N 1 -L ${nodes[0]} hostname
aprun -cc none -n 1 -N 1 -L ${nodes[0]} hostname
aprun -cc none -n 1 -N 1 -L ${nodes[0]} hostname
aprun -cc none -n 1 -N 1 -L ${nodes[0]} hostname

echo "## Bake OFI/GNI:"
aprun -cc none -n 1 -N 1 -L ${nodes[0]} hostname
aprun -cc none -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -cc none -n 1 -N 1 -L ${nodes[0]} bake-mkpool -s 60G /dev/shm/foo.dat
aprun -cc none -n 2 -N 1 -L ${nodes[0]},${nodes[1]} numactl -N 0 -m 0 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "gni://" -p /dev/shm/foo.dat -c 1 

echo "## Bake OFI/GNI (8x concurrency):"
aprun -cc none -n 1 -N 1 -L ${nodes[0]} hostname
aprun -cc none -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -cc none -n 1 -N 1 -L ${nodes[0]} bake-mkpool -s 60G /dev/shm/foo.dat
aprun -cc none -n 2 -N 1 -L ${nodes[0]},${nodes[1]} numactl -N 0 -m 0 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "gni://" -p /dev/shm/foo.dat -c 8 

echo "## Bake OFI/GNI (Hg busy spin):"
aprun -cc none -n 1 -N 1 -L ${nodes[0]} hostname
aprun -cc none -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -cc none -n 1 -N 1 -L ${nodes[0]} bake-mkpool -s 60G /dev/shm/foo.dat
aprun -cc none -n 2 -N 1 -L ${nodes[0]},${nodes[1]} numactl -N 0 -m 0 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "gni://" -p /dev/shm/foo.dat -c 1 -t 0,0

echo "## Bake OFI/GNI (8x concurrency, Hg busy spin):"
aprun -cc none -n 1 -N 1 -L ${nodes[0]} hostname
aprun -cc none -n 1 -N 1 -L ${nodes[0]} rm -f /dev/shm/foo.dat
aprun -cc none -n 1 -N 1 -L ${nodes[0]} bake-mkpool -s 60G /dev/shm/foo.dat
aprun -cc none -n 2 -N 1 -L ${nodes[0]},${nodes[1]} numactl -N 0 -m 0 ./bake-p2p-bw -x 16777216 -m 34359738368 -n "gni://" -p /dev/shm/foo.dat -c 8 -t 0,0


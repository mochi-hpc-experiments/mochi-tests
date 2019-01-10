#!/bin/bash
#SBATCH -N 2
#SBATCH -A startup-carns
#SBATCH --ntasks-per-node=1
#SBATCH --time=15:00
#SBATCH -p bdwall

# example of running benchmarks with spack-compiled software stack

export LMOD_PAGER=none

module load numactl boost cmake

. /home/carns/working/src/spack/share/spack/setup-env.sh

spack load -r ssg
spack load -r bake

module list

# force Intel MPI to use Infiniband instead of Omnipath/PSM2.  If we let 
# Intel MPI use it's preferred PSM2 path then Mercury can't open interface
# because of apparent MR_SCALABLE/MR_BASIC conflict
# we should also try MR_SCALABLE in Mercury NA as a possible workaround
export I_MPI_FABRICS=shm:ofa

echo "## Bake OFI/PSM2 (bdw):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
mpirun ./bake-p2p-bw -x 16777216 -m 34359738368 -n psm2://enp6s0f0:5000 -p /dev/shm/foo.dat -c 1 

echo "## Bake OFI/PSM2 (8x concurrency, bdw):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
mpirun ./bake-p2p-bw -x 16777216 -m 34359738368 -n psm2://enp6s0f0:5000 -p /dev/shm/foo.dat -c 8

echo "## Bake OFI/PSM2 (bdw, Hg busy spin):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
mpirun ./bake-p2p-bw -x 16777216 -m 34359738368 -n psm2://enp6s0f0:5000 -p /dev/shm/foo.dat -c 1 -t 0,0

echo "## Bake OFI/PSM2 (8x concurrency, bdw, Hg busy spin):"
rm -f /dev/shm/foo.dat
bake-mkpool -s 60G /dev/shm/foo.dat
mpirun ./bake-p2p-bw -x 16777216 -m 34359738368 -n psm2://enp6s0f0:5000 -p /dev/shm/foo.dat -c 8 -t 0,0


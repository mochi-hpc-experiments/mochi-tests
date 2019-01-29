#!/bin/bash
#SBATCH -N 1
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

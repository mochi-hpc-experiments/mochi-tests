#!/bin/bash
#SBATCH -N 2
#SBATCH -A radix-io
#SBATCH --ntasks-per-node=8
#SBATCH --time=5:00
#SBATCH -p bdwall

export HOME=$SANDBOX
# example of running benchmarks with spack-compiled software stack

. $SANDBOX/spack/share/spack/setup-env.sh

spack load -r ior

spack find --loaded

# make sure that MPI and libfabric can share PSM2
export PSM2_MULTI_EP=1

export MOBJECT_CLUSTER_FILE=mobject-cluster.gid
echo " ==== running server"
# clean up from any prior runs
srun -n 1 --exclusive rm -f /dev/shm/mobject.dat
srun -n 1 --exclusive mobject-server-daemon ofi+psm2:// $MOBJECT_CLUSTER_FILE &
# backgrounding the 'mobject-server-daemon' means we need to wait just a bit before the cluster is setup
sleep 5
# ... then this confirms the cluster is ready
xxd $MOBJECT_CLUSTER_FILE

echo " ==== running benchmark "
# -g : intra-test barrier
# -t: transfer size
# -b: block size
srun -n 1 --exclusive ior -g -a RADOS -t 64k -b 128k --rados.user=foo --rados.pool=bar --rados.conf $MOBJECT_CLUSTER_FILE

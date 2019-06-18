#!/bin/bash
#SBATCH -N 2
#SBATCH -A startup-carns
#SBATCH --ntasks-per-node=8
#SBATCH --time=5:00
#SBATCH -p bdwall

# example of running benchmarks with spack-compiled software stack

export LMOD_PAGER=none

. $SANDBOX/spack/share/spack/setup-env.sh

spack load -r ior

module list

# make sure that MPI and libfabric can share PSM2
export PSM2_MULTI_EP=1

export MOBJECT_CLUSTER_FILE=mobject-cluster.gid
echo " ==== running server"
# 'sockets' seems to work well, but we are debugging why 'psm2' is a problem
srun -n 4 --exclusive mobject-server-daemon sockets:// $MOBJECT_CLUSTER_FILE &
# backgrounding the 'mobject-server-daemon' means we need to wait just a bit before the cluster is setup
sleep 5
# ... then this confirms the cluster is ready
xxd $MOBJECT_CLUSTER_FILE

echo " ==== running benchmark "
# -g : intra-test barrier
# -t: transfer size
# -b: block size
srun -n 8 --exclusive ior -g -a RADOS -t 64k -b 128k --rados.user=foo --rados.pool=bar --rados.conf $MOBJECT_CLUSTER_FILE

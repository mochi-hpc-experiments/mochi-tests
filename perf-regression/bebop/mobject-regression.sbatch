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

# force Intel MPI to use Infiniband instead of Omnipath/PSM2.  If we let 
# Intel MPI use it's preferred PSM2 path then Mercury can't open interface
# because of apparent MR_SCALABLE/MR_BASIC conflict
# we should also try MR_SCALABLE in Mercury NA as a possible workaround
export I_MPI_FABRICS=shm:ofa



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

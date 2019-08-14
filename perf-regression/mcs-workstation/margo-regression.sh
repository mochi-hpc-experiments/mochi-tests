#!/bin/bash

SANDBOX=$1

. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r ssg 
spack load -r bake

module list

export LD_LIBRARY_PATH=$LIBRARY_PATH

#ldd ./margo-p2p-latency

# NOTE: needed as of January 2018 to avoid conflicts between MPI and 
#       libfabric GNI provider
# NOTE: doing this with -e option to aprun
# NOTE: update as as of September 2018, this is no longer required now that 
#       mercury has switched back to alternative registration method for GNI
# export MPICH_GNI_NDREG_ENTRIES=2000

# echo "## MPI (one way, double the latency for round trip):"
# aprun -n 2 -N 1 ./osu_latency

echo "## Margo OFI/GNI (round trip):"
mpirun -np 2 ./margo-p2p-latency -i 100000 -n tcp
echo "## Margo OFI/GNI (bw, 1MiB):"
mpirun -np 2 ./margo-p2p-bw -x 1048576 -n tcp -c 1 -D 20
echo "## Margo OFI/GNI (bw, 1MiB, 8x concurrency):"
mpirun -np 2 ./margo-p2p-bw -x 1048576 -n tcp -c 8 -D 20
echo "## Margo OFI/GNI (bw, 8MiB):"
mpirun -np 2 ./margo-p2p-bw -x 8388608 -n tcp -c 1 -D 20
echo "## Margo OFI/GNI (bw, 8MiB, 8x concurrency):"
mpirun -np 2 ./margo-p2p-bw -x 8388608 -n tcp -c 8 -D 20
echo "## Margo OFI/GNI (bw, 1MB unaligned):"
mpirun -np 2 ./margo-p2p-bw -x 1000000 -n tcp -c 1 -D 20
echo "## Margo OFI/GNI (bw, 1MB unaligned, 8x concurrency):"
mpirun -np 2 ./margo-p2p-bw -x 1000000 -n tcp -c 8 -D 20


echo "## Margo OFI/GNI (round trip, Hg busy spin):"
mpirun -np 2 ./margo-p2p-latency -i 100000 -n tcp -t 0,0
echo "## Margo OFI/GNI (bw, 1MiB, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 1048576 -n tcp -c 1 -D 20 -t 0,0
echo "## Margo OFI/GNI (bw, 1MiB, 8x concurrency, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 1048576 -n tcp -c 8 -D 20 -t 0,0
echo "## Margo OFI/GNI (bw, 8MiB, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 8388608 -n tcp -c 1 -D 20 -t 0,0
echo "## Margo OFI/GNI (bw, 8MiB, 8x concurrency, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 8388608 -n tcp -c 8 -D 20-t 0,0
echo "## Margo OFI/GNI (bw, 1MB unaligned, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 1000000 -n tcp -c 1 -D 20-t 0,0
echo "## Margo OFI/GNI (bw, 1MB unaligned, 8x concurrency, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 1000000 -n tcp -c 8 -D 20 -t 0,0


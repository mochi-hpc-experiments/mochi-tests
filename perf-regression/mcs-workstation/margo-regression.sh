#!/bin/bash

SANDBOX=$1

. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r ssg 
spack load -r bake
spack load -r mpich

module list

export LD_LIBRARY_PATH=$LIBRARY_PATH

echo "## Margo TCP (round trip):"
mpirun -np 2 ./margo-p2p-latency -i 100000 -n tcp
echo "## Margo TCP (bw, 1MiB):"
mpirun -np 2 ./margo-p2p-bw -x 1048576 -n tcp -c 1 -D 20
echo "## Margo TCP (bw, 1MiB, 8x concurrency):"
mpirun -np 2 ./margo-p2p-bw -x 1048576 -n tcp -c 8 -D 20
echo "## Margo TCP (bw, 8MiB):"
mpirun -np 2 ./margo-p2p-bw -x 8388608 -n tcp -c 1 -D 20
echo "## Margo TCP (bw, 8MiB, 8x concurrency):"
mpirun -np 2 ./margo-p2p-bw -x 8388608 -n tcp -c 8 -D 20
echo "## Margo TCP (bw, 1MB unaligned):"
mpirun -np 2 ./margo-p2p-bw -x 1000000 -n tcp -c 1 -D 20
echo "## Margo TCP (bw, 1MB unaligned, 8x concurrency):"
mpirun -np 2 ./margo-p2p-bw -x 1000000 -n tcp -c 8 -D 20

echo "## Margo TCP (round trip, Hg busy spin):"
mpirun -np 2 ./margo-p2p-latency -i 100000 -n tcp -t 0,0
echo "## Margo TCP (bw, 1MiB, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 1048576 -n tcp -c 1 -D 20 -t 0,0
echo "## Margo TCP (bw, 1MiB, 8x concurrency, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 1048576 -n tcp -c 8 -D 20 -t 0,0
echo "## Margo TCP (bw, 8MiB, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 8388608 -n tcp -c 1 -D 20 -t 0,0
echo "## Margo TCP (bw, 8MiB, 8x concurrency, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 8388608 -n tcp -c 8 -D 20-t 0,0
echo "## Margo TCP (bw, 1MB unaligned, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 1000000 -n tcp -c 1 -D 20-t 0,0
echo "## Margo TCP (bw, 1MB unaligned, 8x concurrency, Hg busy spin):"
mpirun -np 2 ./margo-p2p-bw -x 1000000 -n tcp -c 8 -D 20 -t 0,0


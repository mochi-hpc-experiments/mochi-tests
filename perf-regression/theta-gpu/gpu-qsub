#!/bin/bash
#COBALT -A radix-io
#COBALT -n 2
#COBALT -t 60
#COBALT --mode script
#COBALT -q full-node
#COBALT --attr filesystems=home
# Use -M <mailing address> to receive email about job status if desired
# Use --attr <system's scratch area> to run this script if desired

# exit on any error
set -e

# proxy settings in case the node doesn't have outbound network connectivity.
# See ANL website for info.
export HTTP_PROXY=http://theta-proxy.tmi.alcf.anl.gov:3128
export HTTPS_PROXY=http://theta-proxy.tmi.alcf.anl.gov:3128
export http_proxy=http://theta-proxy.tmi.alcf.anl.gov:3128
export https_proxy=http://theta-proxy.tmi.alcf.anl.gov:3128
#
#
# note: disable registration cache for verbs provider for now; see
#       discussion in https://github.com/ofiwg/libfabric/issues/5244
export FI_MR_CACHE_MAX_COUNT=0
# use shared recv context in RXM; should improve scalability
export FI_OFI_RXM_USE_SRX=1

echo "=== CREATE DIRECTORIES AND DOWNLOAD CODE ==="

# Copy mochi-tests/perf-regression/theta-gpu/gpu-qsub to the location
# where you want to run the tests.
ORIGIN=$PWD
# Scratch area for builds
SANDBOX=$PWD/sb-gpu-$$
PREFIX=$SANDBOX/install

# modify HOME env variable so that we don't perturb ~/.spack/ files for the
# users calling this script
export HOME=$SANDBOX
mkdir $SANDBOX
mkdir $PREFIX
mkdir $PREFIX/bin

cd $SANDBOX
git clone -q https://github.com/spack/spack.git
git clone -q https://github.com/mochi-hpc/mochi-spack-packages.git
git clone -q https://github.com/mochi-hpc-experiments/mochi-tests.git
git clone -q https://github.com/mochi-hpc-experiments/platform-configurations.git

echo "=== SET UP SPACK ENVIRONMENT ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack env create gpu-mochi-regression $SANDBOX/platform-configurations/ANL/ThetaGPU/spack.yaml
spack env activate gpu-mochi-regression
spack repo rm /path/to/mochi-spack-packages
spack repo add $SANDBOX/mochi-spack-packages
spack add mochi-ssg@develop ^openmpi
spack install
spack find

#Somehow spack env didn't activate/load the view environment in LD_LIBRARY_PATH
spack view --verbose symlink $PREFIX mochi-margo mochi-ssg mercury argobots
export LD_LIBRARY_PATH=$PREFIX/lib:$LD_LIBRARY_PATH
#Set paths for openmpi and nvcc
export PATH=/lus/theta-fs0/software/thetagpu/openmpi-4.0.5/bin:/usr/local/cuda/bin/:$PATH

# mochi-tests
echo "=== BUILD TEST PROGRAMS ==="
cd $SANDBOX/mochi-tests

autoreconf -fi -Im4
mkdir ./BUILD
cd BUILD
../configure CC=mpicc
make

echo "=== RUN TESTS ==="
#run test on 2 different hosts
FIRST=$(cat $COBALT_NODEFILE | sed -n '1p')
SECOND=$(cat $COBALT_NODEFILE | sed -n '2p')

echo "=== GPU memory of host $FIRST to GPU memory of host $SECOND ==="
mpiexec -n 2 -host $FIRST,$SECOND $SANDBOX/mochi-tests/BUILD/perf-regression/gpu-margo-p2p-bw -x 4096 -n "ofi+verbs://" -c 1 -D 10

echo "=== From GPU memory of host $FIRST to main memory of host $SECOND =="
mpiexec -n 2 -host $FIRST,$SECOND $SANDBOX/mochi-tests/BUILD/perf-regression/gpu-margo-p2p-bw -x 4096 -n "ofi+verbs://" -c 1 -D 10 -j

echo "=== From main memory of host $FIRST to GPU memory of host $SECOND =="
mpiexec -n 2 -host $FIRST,$SECOND $SANDBOX/mochi-tests/BUILD/perf-regression/gpu-margo-p2p-bw -x 4096 -n "ofi+verbs://" -c 1 -D 10 -k

echo "=== JOB DONE==="
echo "See result in $COBALT_JOBID.output/error/cobaltlog in $ORIGIN"
echo "Remove scratch area for builds and tests in $SANDBOX as needed"

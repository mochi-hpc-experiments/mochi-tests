#!/bin/bash

# This is a shell script to be run from a login node of the Polaris system at
# the ALCF, that will download, compile, and execute Mochi performance
# regression tests

# exit on any error
set -e

# ignore return code on these in case the right modules are already loaded
module swap PrgEnv-nvhpc PrgEnv-gnu || true
module load cudatoolkit-standalone || true

echo "=== CREATE DIRECTORIES AND DOWNLOAD CODE ==="

# location of this script
ORIGIN=$PWD
# scratch area
SANDBOX=$PWD/sandbox-$$
# install destination
PREFIX=$SANDBOX/install
# modify HOME env variable so that we don't perturb ~/.spack/ files for the
# users calling this script
export HOME=$SANDBOX
mkdir $SANDBOX
mkdir $PREFIX
mkdir $PREFIX/bin
cp $ORIGIN/*.qsub $PREFIX/bin

cd $SANDBOX
git clone -q https://github.com/spack/spack.git
git clone -q https://github.com/mochi-hpc/mochi-spack-packages.git
git clone -q https://github.com/mochi-hpc-experiments/mochi-tests.git
git clone -q https://github.com/mochi-hpc-experiments/platform-configurations.git

echo "=== SET UP SPACK ENVIRONMENT ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack env create mochi-regression $SANDBOX/platform-configurations/ANL/Polaris/spack.yaml
spack env activate mochi-regression
spack repo add $SANDBOX/mochi-spack-packages
spack add mochi-ssg+mpi
# install initial packages
spack install

# mochi-tests
echo "=== BUILD TEST PROGRAMS ==="
cd $SANDBOX/mochi-tests
./prepare.sh
mkdir build
cd build
echo ../configure --prefix=$PREFIX CC=cc
../configure --prefix=$PREFIX CC=cc
make -j 3
make install

echo "=== SUBMIT AND WAIT FOR JOBS ==="
cd $PREFIX/bin
export SANDBOX

# continue on these even if a job fails so that we get emails with partial
# results
qsub -W block=true -v SANDBOX -o gpu.out -e gpu.err ./run_gpu_margo_p2p_bw.qsub || true
qsub -W block=true -v SANDBOX -o margo.out -e margo.err ./margo-regression.qsub || true
qsub -W block=true -v SANDBOX -o vector.out -e vector.err ./margo-vector-regression.qsub || true

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat margo.err margo.out vector.err vector.out gpu.err gpu.out > combined.$JOBID.txt
dos2unix combined.$JOBID.txt
mailx -s "mochi-regression (polaris)" sds-commits@lists.mcs.anl.gov < combined.$JOBID.txt
cat combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX


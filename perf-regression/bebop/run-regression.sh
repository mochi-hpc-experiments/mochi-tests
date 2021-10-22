#!/bin/bash

# This is a shell script to be run from a login node of the Bebop system at
# the LCRC, that will download, compile, and execute mochi performance
# regression tests

# exit on any error
set -e

# use modern gcc
module load gcc

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
cp $ORIGIN/margo-regression.sbatch $PREFIX/bin
cp $ORIGIN/margo-vector-regression.sbatch $PREFIX/bin
cp $ORIGIN/bake-regression.sbatch $PREFIX/bin
cp $ORIGIN/pmdk-regression.sbatch $PREFIX/bin
# cp $ORIGIN/mobject-regression.sbatch $JOBDIR

cd $SANDBOX
git clone -q https://github.com/spack/spack.git
git clone -q https://github.com/mochi-hpc/mochi-spack-packages.git
git clone -q https://github.com/mochi-hpc-experiments/mochi-tests.git
git clone -q https://github.com/mochi-hpc-experiments/platform-configurations.git

echo "=== SET UP SPACK ENVIRONMENT ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack env create mochi-regression $SANDBOX/platform-configurations/ANL/Bebop/spack.yaml
spack env activate mochi-regression
spack repo rm /path/to/mochi-spack-packages
spack repo add $SANDBOX/mochi-spack-packages
# install initial packages specified in bebop environment
spack install
# add additional packages needed for performance regression tests and install
# NOTE: as of 2021-10-20, the Spack concretizer does not produce a correct
#       solution if we add these before the first spack install
spack add mochi-ssg+mpi
spack add mochi-bake
# as of 2021-05-29 this isn't building right for some reason; skip the
# mobject tests
# spack add ior@master +mobject
spack install

# mochi-tests
echo "=== BUILD TEST PROGRAMS ==="
cd $SANDBOX/mochi-tests
./prepare.sh
mkdir build
cd build
echo ../configure --prefix=$PREFIX CC=mpicc
../configure --prefix=$PREFIX CC=mpicc
make -j 3
make install

echo "=== SUBMIT AND WAIT FOR JOBS ==="
cd $PREFIX/bin
export SANDBOX
sbatch --wait --export=ALL ./margo-regression.sbatch || true
sbatch --wait --export=ALL ./margo-vector-regression.sbatch || true
sbatch --wait --export=ALL ./bake-regression.sbatch || true
sbatch --wait --export=ALL ./pmdk-regression.sbatch || true

# sbatch --wait --export=ALL ./mobject-regression.sbatch || true

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat *.out | col -b > combined.$SLURM_JOB_ID.txt
# dos2unix combined.$SLURM_JOB_ID.txt
mailx -s "mochi-regression (bebop)" sds-commits@lists.mcs.anl.gov < combined.$SLURM_JOB_ID.txt

cd /tmp
rm -rf $SANDBOX

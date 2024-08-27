#!/bin/bash

# compile and set up anything needed for regression jobs

# exit on any error
set -e

if [ -z $1 ]; then
    echo "Error: no path given."
    exit 1
fi

# scratch area to install in
SANDBOX=`realpath $1`
# location of this script
ORIGIN=$(dirname "$0")

# choose preferred modules
# ignore return code on these in case the right modules are already loaded
module swap PrgEnv-nvhpc PrgEnv-gnu || true
module load nvhpc-mixed || true

echo "=== CREATE DIRECTORIES AND DOWNLOAD CODE ==="

# install destination
PREFIX=$SANDBOX/install
# modify HOME env variable so that we don't perturb ~/.spack/ files for the
# users calling this script
export HOME=$SANDBOX
mkdir -p $SANDBOX
mkdir -p $PREFIX
mkdir -p $PREFIX/bin
cp $ORIGIN/*.qsub $PREFIX/bin

cd $SANDBOX
git clone -q https://github.com/spack/spack.git
rm -rf spack/.git*
git clone -q https://github.com/mochi-hpc/mochi-spack-packages.git
git clone -q https://github.com/mochi-hpc-experiments/mochi-tests.git
git clone -q https://github.com/mochi-hpc-experiments/platform-configurations.git

echo "=== SET UP SPACK ENVIRONMENT ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack env create mochi-regression $SANDBOX/platform-configurations/ANL/Polaris/spack.yaml
spack env activate mochi-regression
spack mirror remove mochi-buildcache
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


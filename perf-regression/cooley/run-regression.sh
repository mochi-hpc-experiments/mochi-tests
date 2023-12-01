#!/bin/bash

# This is a shell script to be run from a login node of the Cooley system at
# the ALCF, that will download, compile, and execute Mochi performance
# regression tests

# exit on any error
set -e

source /etc/profile.d/00softenv.sh
soft add +gcc-8.2.0
soft add +cmake-3.20.4
soft add +mvapich2

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
spack env create mochi-regression $SANDBOX/platform-configurations/ANL/Cooley/spack.yaml
spack env activate mochi-regression
spack repo add $SANDBOX/mochi-spack-packages
# modify variants to allow for testing of bmi+tcp, ofi+tcp, and ofi+sockets
spack add libfabric fabrics=rxm,verbs,tcp,sockets
spack add mercury ~boostsys ~checksum +bmi
# install initial packages
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

JOBID=`qsub --env SANDBOX=$SANDBOX ./margo-regression.qsub`
cqwait $JOBID
JOBID2=`qsub --env SANDBOX=$SANDBOX ./bake-regression.qsub`
cqwait $JOBID2
JOBID3=`qsub --env SANDBOX=$SANDBOX ./pmdk-regression.qsub`
cqwait $JOBID3
JOBID4=`qsub --env SANDBOX=$SANDBOX ./margo-regression-tcp.qsub`
cqwait $JOBID4
JOBID5=`qsub --env SANDBOX=$SANDBOX ./margo-vector-regression.qsub`
cqwait $JOBID5

# TODO: commented out on 2020-08-13 due to build problem
# JOBID4=`qsub --env SANDBOX=$SANDBOX ./mobject-regression.qsub`
# cqwait $JOBID4
# JOBID5=`qsub --env SANDBOX=$SANDBOX ./bake-kove.qsub`
# cqwait $JOBID5

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat $JOBID.* $JOBID2.* $JOBID3.* $JOBID4.* $JOBID5.* > combined.$JOBID.txt
dos2unix combined.$JOBID.txt
mailx -s "mochi-regression (cooley)" sds-commits@lists.mcs.anl.gov < combined.$JOBID.txt
cat combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX


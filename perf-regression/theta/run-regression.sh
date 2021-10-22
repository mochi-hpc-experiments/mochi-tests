#!/bin/bash

# This is a shell script to be run from a login node of the Theta system at
# the ALCF, that will download, compile, and execute the Mochi performance
# regression tests

# exit on any error
set -e

# use gnu compilers
module swap PrgEnv-intel PrgEnv-gnu
module load gcc

# dynamic link everything by default
export CRAYPE_LINK_TYPE=dynamic

echo "=== CREATE DIRECTORIES AND DOWNLOAD CODE ==="

# location of this script
ORIGIN=$PWD
# scratch area for builds
if [ -z "$WORKSPACE_TMP" ]; then
    SANDBOX=$PWD/sb-$$
else
    # if that variable is set and not empty, then we are in a Jenkins
    # environment.  Put the sandbox within the temp dir specified by Jenkins
    # as a workaround to the long path problem described in
    # https://github.com/spack/spack/issues/22548
    SANDBOX=$WORKSPACE_TMP/sb-$$
fi

PREFIX=$SANDBOX/install
# modify HOME env variable so that we don't perturb ~/.spack/ files for the
# users calling this script
export HOME=$SANDBOX
mkdir $SANDBOX
mkdir $PREFIX
mkdir $PREFIX/bin
cp ${ORIGIN}/*.qsub $PREFIX/bin


cd $SANDBOX
git clone -q https://github.com/spack/spack.git
git clone -q https://github.com/mochi-hpc/mochi-spack-packages.git
git clone -q https://github.com/mochi-hpc-experiments/mochi-tests.git
git clone -q https://github.com/mochi-hpc-experiments/platform-configurations.git

echo "=== SET UP SPACK ENVIRONMENT ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack env create mochi-regression $SANDBOX/platform-configurations/ANL/Theta/spack.yaml
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
../configure --prefix=$PREFIX CC=cc
make -j 3
make install

echo "=== SUBMIT AND WAIT FOR JOBS ==="
cd $PREFIX/bin
JOBID=`qsub --env SANDBOX=$SANDBOX ./margo-regression.qsub`
cqwait $JOBID
JOBID2=`qsub --env SANDBOX=$SANDBOX ./bake-regression.qsub`
cqwait $JOBID2
JOBID3=`qsub --env SANDBOX=$SANDBOX ./pmdk-regression.qsub`
cqwait $JOBID3
# JOBID4=`qsub --env SANDBOX=$SANDBOX ./mobject-regression.qsub`
# cqwait $JOBID4
JOBID5=`qsub --env SANDBOX=$SANDBOX ./separate-ssg.qsub`
cqwait $JOBID5
JOBID6=`qsub --env SANDBOX=$SANDBOX ./margo-vector-regression.qsub`
cqwait $JOBID6

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
# cat $JOBID.* $JOBID2.* $JOBID3.* $JOBID4.* $JOBID5.* $JOBID6.* > combined.$JOBID.txt
cat $JOBID.* $JOBID2.* $JOBID3.* $JOBID5.* $JOBID6.* > combined.$JOBID.txt
dos2unix combined.$JOBID.txt
mailx -r sds-commits@mcs.anl.gov -s "mochi-regression (theta)" sds-commits@mcs.anl.gov < combined.$JOBID.txt
cat combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX


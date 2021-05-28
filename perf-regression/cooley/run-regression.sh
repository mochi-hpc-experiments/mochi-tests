#!/bin/bash

# This is a shell script to be run from a login node of the Cooley system at
# the ALCF, that will download, compile, and execute Mochi performance 
# regression tests, including any dependencies

# exit on any error
set -e

# grab helpful things (especially newer compiler) from softenv
source /etc/profile.d/00softenv.sh
soft add +gcc-8.2.0
soft add +cmake-3.9.1
soft add +mvapich2

# location of this script
ORIGIN=$PWD
# scratch area for builds
SANDBOX=$PWD/sandbox-$$
# install destination
PREFIX=$PWD/mochi-regression-install-$$
# job submission dir
JOBDIR=$PWD/mochi-regression-job-$$
# modify HOME env variable so that we don't perturb ~/.spack/ files for the 
# users calling this script
export HOME=$SANDBOX

mkdir $SANDBOX
mkdir $PREFIX
mkdir $JOBDIR
cp $ORIGIN/margo-regression.qsub $JOBDIR
cp $ORIGIN/margo-vector-regression.qsub $JOBDIR
cp $ORIGIN/bake-regression.qsub $JOBDIR
cp $ORIGIN/pmdk-regression.qsub $JOBDIR
cp $ORIGIN/mobject-regression.qsub $JOBDIR

# Skipping Kove tests as of April 2020
# cp $ORIGIN/bake-kove.qsub $JOBDIR

# set up build environment
cd $SANDBOX
git clone -q https://github.com/spack/spack.git
# Using origin/develop of spack as of 3-22-2021.  The current release
# (0.16.1) will not return an error code if spack install fails.
# (cd spack && git checkout -b spack-0.16.1 v0.16.1)
git clone -q https://github.com/mochi-hpc/mochi-spack-packages.git
git clone -q https://github.com/mochi-hpc-experiments/mochi-tests.git

echo "=== BUILD SPACK PACKAGES AND LOAD ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack compiler find
spack compilers

# use our own packages.yaml for cooley-specific preferences
cp $ORIGIN/packages.yaml $SPACK_ROOT/etc/spack
# add external repo for mochi.  Note that this will not modify the
# user's ~/.spack/ files because we modified $HOME above
spack repo add ${SANDBOX}/mochi-spack-packages
# sanity check
spack repo list
# clean out any stray packages from previous runs, just in case
spack uninstall -R -y argobots mercury rdma-core libfabric || true
# ior acts as our "apex" package here, causing several other packages to build
# as of 2021-03-22 this isn't building right for some reason; skip the
# mobject tests
# spack install ior@master +mobject ^mochi-ssg@main
spack install mochi-bake
# deliberately repeat setup-env step after building modules to ensure
#   that we pick up the right module paths
. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r mochi-bake

# mochi-tests
echo "=== BUILDING SDS TEST PROGRAMS ==="
cd $SANDBOX/mochi-tests
libtoolize
./prepare.sh
mkdir build
cd build
../configure --prefix=$PREFIX CC=mpicc
make -j 3
make install

# mercury-runner benchmark
# echo "=== BUILDING MERCURY-RUNNER BENCHMARK ==="
# cd $SANDBOX/mercury-runner
# mkdir build
# cd build
# CC=mpicc CXX=mpicxx CXXFLAGS='-D__STDC_FORMAT_MACROS' cmake -DCMAKE_PREFIX_PATH=$PREFIX -DCMAKE_INSTALL_PREFIX=$PREFIX -DMPI=ON ..
# make -j 3
# make install

# set up job to run
echo "=== SUBMITTING AND WAITING FOR JOB ==="
cp $PREFIX/bin/margo-p2p-latency $JOBDIR
cp $PREFIX/bin/margo-p2p-bw $JOBDIR
cp $PREFIX/bin/margo-p2p-vector $JOBDIR
cp $PREFIX/bin/bake-p2p-bw $JOBDIR
cp $PREFIX/bin/pmdk-bw $JOBDIR
# cp $PREFIX/bin/mercury-runner $JOBDIR
cd $JOBDIR

JOBID=`qsub --env SANDBOX=$SANDBOX ./margo-regression.qsub`
cqwait $JOBID
JOBID2=`qsub --env SANDBOX=$SANDBOX ./bake-regression.qsub`
cqwait $JOBID2
JOBID3=`qsub --env SANDBOX=$SANDBOX ./pmdk-regression.qsub`
cqwait $JOBID3
# TODO: commented out on 2020-08-13 due to build problem
# JOBID4=`qsub --env SANDBOX=$SANDBOX ./mobject-regression.qsub`
# cqwait $JOBID4
# JOBID5=`qsub --env SANDBOX=$SANDBOX ./bake-kove.qsub`
# cqwait $JOBID5
JOBID6=`qsub --env SANDBOX=$SANDBOX ./margo-vector-regression.qsub`
cqwait $JOBID6

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat $JOBID.* $JOBID2.* $JOBID3.* $JOBID6.* > combined.$JOBID.txt
dos2unix combined.$JOBID.txt
mailx -s "mochi-regression (cooley)" sds-commits@lists.mcs.anl.gov < combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX
rm -rf $PREFIX

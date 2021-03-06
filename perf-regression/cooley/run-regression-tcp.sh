#!/bin/bash

# This is a shell script to be run from a login node of the Cooley system at
# the ALCF, that will download, compile, and execute Mochi performance 
# regression tests, including any dependencies

# This variation is meant exclusively to test TCP performance using all
# available methods in Mercury.

# exit on any error
set -e

# grab helpful things (especially newer compiler) from softenv
source /etc/profile.d/00softenv.sh
soft add +gcc-8.2.0
soft add +autotools-feb2016
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
cp $ORIGIN/margo-regression-tcp.qsub $JOBDIR

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
cp $ORIGIN/packages-tcp.yaml $SPACK_ROOT/etc/spack/packages.yaml
# make sure that LD_LIBRARY_PATH will be set
cp $ORIGIN/modules.yaml $SPACK_ROOT/etc/spack
# build in /tmp
cp $ORIGIN/config.yaml $SPACK_ROOT/etc/spack
# add external repo for mochi.  Note that this will not modify the 
# user's ~/.spack/ files because we modified $HOME above
spack repo add ${SANDBOX}/mochi-spack-packages
# sanity check
spack repo list
# clean out any stray packages from previous runs, just in case
spack uninstall -R -y argobots mercury rdma-core libfabric bmi || true
# ssg acts as our "apex" package here, causing several other packages to build
spack install mochi-ssg
# deliberately repeat setup-env step after building modules to ensure
#   that we pick up the right module paths
. $SANDBOX/spack/share/spack/setup-env.sh

spack load -r mochi-ssg

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

# set up job to run
echo "=== SUBMITTING AND WAITING FOR JOB ==="
cp $PREFIX/bin/margo-p2p-latency $JOBDIR
cp $PREFIX/bin/margo-p2p-bw $JOBDIR
cd $JOBDIR

JOBID=`qsub --env SANDBOX=$SANDBOX ./margo-regression-tcp.qsub`
cqwait $JOBID

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat $JOBID.* > combined.$JOBID.txt
dos2unix combined.$JOBID.txt
mailx -s "mochi-regression (cooley, TCP/IP)" sds-commits@lists.mcs.anl.gov < combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX
rm -rf $PREFIX

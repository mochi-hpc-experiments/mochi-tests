#!/bin/bash

# This is a shell script to be run from a login node of the Theta system at
# the ALCF, that will download, compile, and execute the Mochi performance 
# regression tests, including any dependencies

# exit on any error
set -e

# set up environment
# use gnu compilers
module swap PrgEnv-intel PrgEnv-gnu
module load cce
# default to O3 optimizations unless otherwise specified
export CFLAGS="-O3"
# dynamic link everything by default
export CRAYPE_LINK_TYPE=dynamic

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

# install destination
PREFIX=$PWD/mochi-regression-install-$$
# job submission dir
JOBDIR=$PWD/mochi-regression-job-$$

mkdir -p $SANDBOX
mkdir $PREFIX
mkdir $JOBDIR
ls *.qsub
cp ${ORIGIN}/*.qsub ${JOBDIR}

# modify HOME env variable so that we don't perturb ~/.spack/ files for the 
# users calling this script
export HOME=$SANDBOX

# set up build environment
cd $SANDBOX
git clone -q https://github.com/spack/spack.git
# Using origin/develop as of 2021-05-03
# (cd spack && git checkout -b spack-0.16.0 v0.16.0)
git clone -q https://github.com/mochi-hpc/mochi-spack-packages.git
git clone -q https://github.com/mochi-hpc-experiments/mochi-tests.git

echo "=== BUILD SPACK PACKAGES AND LOAD ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack compiler find
spack compilers

# use our own configuration file for theta-specific preferences
cp $ORIGIN/packages.yaml $SPACK_ROOT/etc/spack
# note: the following is a workaround to make sure that LD_LIBRARY_PATH is
# set on Cray platforms, see https://github.com/spack/spack/issues/23228
cp $ORIGIN/modules.yaml $SPACK_ROOT/etc/spack
cp $ORIGIN/config.yaml $SPACK_ROOT/etc/spack
# add external repo for mochi.  Note that this will not modify the 
# user's ~/.spack/ files because we modified $HOME above
spack repo add ${SANDBOX}/mochi-spack-packages
# sanity check
spack repo list
# clean out any stray packages from previous runs, just in case
spack uninstall -R -y argobots mercury libfabric || true
# ior acts as our "apex" package here, causing several other packages to build
# TODO: temporarily disabling mobject tests, 2021-05-28
# spack spec ior@master +mobject ^mochi-ssg@main^mpich
# spack install ior@master +mobject ^mochi-ssg@main^mpich
spack install mochi-bake

# deliberately repeat setup-env step after building modules to ensure
#   that we pick up the right module paths
. $SANDBOX/spack/share/spack/setup-env.sh
# load ssg and bake because they are needed by things compiled outside of
# spack later in this script
# spack load -r ior
spack load -r mochi-bake

# mochi-tests
echo "=== BUILDING SDS TEST PROGRAMS ==="
cd $SANDBOX/mochi-tests
./prepare.sh
mkdir build
cd build
../configure --prefix=$PREFIX CC=cc
make -j 3
make install

# set up job to run
echo "=== SUBMITTING AND WAITING FOR JOB ==="
cp $PREFIX/bin/margo-p2p-latency $JOBDIR
cp $PREFIX/bin/margo-p2p-bw $JOBDIR
cp $PREFIX/bin/margo-p2p-vector $JOBDIR
cp $PREFIX/bin/bake-p2p-bw $JOBDIR
cp $PREFIX/bin/pmdk-bw $JOBDIR
cp $PREFIX/bin/ssg-test-separate-group-create $JOBDIR
cp $PREFIX/bin/ssg-test-separate-group-attach $JOBDIR
cd $JOBDIR

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
#dos2unix combined.$JOBID.txt
mailx -r sds-commits@mcs.anl.gov -s "mochi-regression (theta)" sds-commits@mcs.anl.gov < combined.$JOBID.txt
cat combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX
rm -rf $PREFIX

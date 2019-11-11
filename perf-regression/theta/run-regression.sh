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
SANDBOX=$PWD/mochi-regression-sandbox-$$
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
cp $ORIGIN/bake-regression.qsub $JOBDIR
cp $ORIGIN/pmdk-regression.qsub $JOBDIR

# set up build environment
cd $SANDBOX
git clone https://github.com/spack/spack.git
git clone https://xgitlab.cels.anl.gov/sds/sds-repo.git
git clone https://xgitlab.cels.anl.gov/sds/sds-tests.git

echo "=== BUILD SPACK PACKAGES AND LOAD ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack compiler find
spack compilers

# use our own configuration file for theta-specific preferences
cp $ORIGIN/packages.yaml $SPACK_ROOT/etc/spack
cp $ORIGIN/config.yaml $SPACK_ROOT/etc/spack
# add external repo for mochi.  Note that this will not modify the 
# user's ~/.spack/ files because we modified $HOME above
spack repo add ${SANDBOX}/sds-repo
# sanity check
spack repo list
# clean out any stray packages from previous runs, just in case
spack uninstall -R -y argobots mercury libfabric || true
# ior acts as our "apex" package here, causing several other packages to build
#spack spec ior@develop +mobject
#spack install ior@develop +mobject
# TODO: above temporarily disabled because
#       Mobject and the sds-tests benchmarks require different versions of SSG.
spack spec bake
spack spec ssg ^mpich
spack install bake
spack install ssg ^mpich


# deliberately repeat setup-env step after building modules to ensure
#   that we pick up the right module paths
. $SANDBOX/spack/share/spack/setup-env.sh
# load ssg and bake because they are needed by things compiled outside of
# spack later in this script
spack load -r ssg
spack load -r bake

# sds-tests
echo "=== BUILDING SDS TEST PROGRAMS ==="
cd $SANDBOX/sds-tests
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
cp $PREFIX/bin/bake-p2p-bw $JOBDIR
cp $PREFIX/bin/pmdk-bw $JOBDIR
cd $JOBDIR

JOBID=`qsub --env SANDBOX=$SANDBOX ./margo-regression.qsub`
cqwait $JOBID
JOBID2=`qsub --env SANDBOX=$SANDBOX ./bake-regression.qsub`
cqwait $JOBID2
JOBID3=`qsub --env SANDBOX=$SANDBOX ./pmdk-regression.qsub`
cqwait $JOBID3
# cannot run mobject until updated to match ssg group changes
#JOBID4=`qsub --env SANDBOX=$SANDBOX ./mobject-regression.qsub`
#cqwait $JOBID4

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat $JOBID.* $JOBID2.* $JOBID3.* > combined.$JOBID.txt
#dos2unix combined.$JOBID.txt
mailx -r sds-commits@mcs.anl.gov -s "mochi-regression (theta)" carns@mcs.anl.gov < combined.$JOBID.txt
cat combined.$JOBID.txt

cd /tmp
# rm -rf $SANDBOX
# rm -rf $PREFIX

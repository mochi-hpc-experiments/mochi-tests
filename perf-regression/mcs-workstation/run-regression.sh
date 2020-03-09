#!/bin/bash

# This is a shell script to be run from an MCS workstation
# that will download, compile, and execute the Mochi performance 
# regression tests, including any dependencies

# exit on any error
set -e

# default to O3 optimizations unless otherwise specified
export CFLAGS="-O3"

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
mkdir -p $HOME/.spack/linux

cp $ORIGIN/margo-regression.sh $JOBDIR
cp $ORIGIN/bake-regression.sh $JOBDIR
cp $ORIGIN/pmdk-regression.sh $JOBDIR

# set up build environment
cd $SANDBOX
git clone -q https://github.com/spack/spack.git
(cd spack && git checkout -b spack-0.14 v0.14.0)
git clone -q https://xgitlab.cels.anl.gov/sds/sds-repo.git
git clone -q https://xgitlab.cels.anl.gov/sds/sds-tests.git

echo "=== BUILD SPACK PACKAGES AND LOAD ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack compiler find
spack compilers

# use our own packages.yaml for the workstation-specific preferences
cp $ORIGIN/packages.yaml $SPACK_ROOT/etc/spack
cp $ORIGIN/compilers.yaml $HOME/.spack/linux
# add external repo for mochi.  Note that this will not modify the 
# user's ~/.spack/ files because we modified $HOME above
spack repo add ${SANDBOX}/sds-repo
# sanity check
spack repo list
# bootstrap spack to get environment modules
spack bootstrap
. $SANDBOX/spack/share/spack/setup-env.sh
# clean out any stray packages from previous runs, just in case
spack uninstall -R -y argobots mercury libfabric || true
# ior acts as our "apex" package here, causing several other packages to build
spack install ior@develop +mobject
# deliberately repeat setup-env step after building modules to ensure
#   that we pick up the right module paths
. $SANDBOX/spack/share/spack/setup-env.sh
# load ssg and bake because they are needed by things compiled outside of
# spack later in this script
spack load -r mochi-ssg
spack load -r mochi-bake
spack load -r mpich

# sds-tests
echo "=== BUILDING SDS TEST PROGRAMS ==="
cd $SANDBOX/sds-tests
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
cp $PREFIX/bin/bake-p2p-bw $JOBDIR
cp $PREFIX/bin/pmdk-bw $JOBDIR
cd $JOBDIR

./margo-regression.sh $SANDBOX | tee > margo-results.txt
./bake-regression.sh $SANDBOX  | tee > bake-results.txt
./pmdk-regression.sh $SANDBOX  | tee > pmdk-results.txt

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat *-results.txt > combined.txt
#dos2unix combined.$JOBID.txt
mailx -r mdorier@anl.gov -s "mochi-regression (MCS workstation)" sds-commits@lists.mcs.anl.gov < combined.txt
cat combined.txt

cd /tmp
rm -rf $SANDBOX
rm -rf $PREFIX

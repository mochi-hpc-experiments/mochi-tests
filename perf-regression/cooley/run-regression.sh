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
cp $ORIGIN/margo-vector-regression.qsub $JOBDIR
cp $ORIGIN/bake-regression.qsub $JOBDIR
cp $ORIGIN/pmdk-regression.qsub $JOBDIR
cp $ORIGIN/mobject-regression.qsub $JOBDIR

# Skipping Kove tests as of April 2020
# cp $ORIGIN/bake-kove.qsub $JOBDIR

# set up build environment
cd $SANDBOX
git clone -q https://github.com/spack/spack.git
(cd spack && git checkout -b spack-0.15.0 v0.15.0)
git clone -q https://xgitlab.cels.anl.gov/sds/sds-repo.git
git clone -q https://xgitlab.cels.anl.gov/sds/sds-tests.git

echo "=== BUILD SPACK PACKAGES AND LOAD ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack compiler find
spack compilers

# use our own packages.yaml for cooley-specific preferences
cp $ORIGIN/packages.yaml $SPACK_ROOT/etc/spack
# add external repo for mochi.  Note that this will not modify the 
# user's ~/.spack/ files because we modified $HOME above
spack repo add ${SANDBOX}/sds-repo
# sanity check
spack repo list
# underlying tools needed by spack
spack bootstrap
# clean out any stray packages from previous runs, just in case
spack uninstall -R -y argobots mercury rdma-core libfabric || true
# ior acts as our "apex" package here, causing several other packages to build
spack install ior@master +mobject
spack install mochi-ssg
spack install mochi-bake
# check what stable version of bake we got
BAKE_STABLE_VER=`spack find mochi-bake |grep mochi-bake |grep -v file-backend`
# load an additional version of bake that uses a file backend
spack install mochi-bake@dev-file-backend
# deliberately repeat setup-env step after building modules to ensure
#   that we pick up the right module paths
. $SANDBOX/spack/share/spack/setup-env.sh
# load ssg and bake because they are needed by things compiled outside of
# spack later in this script
spack load -r mochi-ssg
spack load -r $BAKE_STABLE_VER

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

# switch bake versions and build another copy with file backend
echo "=== BUILDING SDS TEST PROGRAMS WITH FILE BACKEND ==="
spack unload $BAKE_STABLE_VER
spack load -r mochi-bake@dev-file-backend
cd $SANDBOX/sds-tests
mkdir build-file
cd build-file
../configure --prefix=${PREFIX}-file CC=mpicc
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
cp ${PREFIX}-file/bin/bake-p2p-bw $JOBDIR/bake-p2p-bw-file
cp $PREFIX/bin/pmdk-bw $JOBDIR
# cp $PREFIX/bin/mercury-runner $JOBDIR
cd $JOBDIR

JOBID=`qsub --env SANDBOX=$SANDBOX ./margo-regression.qsub`
cqwait $JOBID
JOBID2=`qsub --env SANDBOX=$SANDBOX ./bake-regression.qsub`
cqwait $JOBID2
JOBID3=`qsub --env SANDBOX=$SANDBOX ./pmdk-regression.qsub`
cqwait $JOBID3
JOBID4=`qsub --env SANDBOX=$SANDBOX ./mobject-regression.qsub`
cqwait $JOBID4
# JOBID5=`qsub --env SANDBOX=$SANDBOX ./bake-kove.qsub`
# cqwait $JOBID5
JOBID6=`qsub --env SANDBOX=$SANDBOX ./margo-vector-regression.qsub`
cqwait $JOBID6

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat $JOBID.* $JOBID2.* $JOBID3.* $JOBID4.* $JOBID6.* > combined.$JOBID.txt
dos2unix combined.$JOBID.txt
mailx -s "mochi-regression (cooley)" sds-commits@lists.mcs.anl.gov < combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX
rm -rf $PREFIX

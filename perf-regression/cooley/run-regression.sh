#!/bin/bash

# This is a shell script to be run from a login node of the Cooley system at
# the ALCF, that will download, compile, and execute Mochi performance 
# regression tests, including any dependencies

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
cp $ORIGIN/mobject-regression.qsub $JOBDIR
cp $ORIGIN/bake-kove.qsub $JOBDIR

# set up build environment
cd $SANDBOX
git clone https://github.com/spack/spack.git
git clone https://xgitlab.cels.anl.gov/sds/sds-repo.git
git clone https://xgitlab.cels.anl.gov/sds/sds-tests.git

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
# TODO: temporary, for now just testing bake and margo until mobject is
#       updated to use same version of ssg 2019-11-11
# spack install ior@develop +mobject
spack install ssg
spack install bake
# check what stable version of bake we got
BAKE_STABLE_VER=`spack find bake |grep bake |grep -v file-backend`
# load an additional version of bake that uses a file backend
spack install bake@dev-file-backend
# deliberately repeat setup-env step after building modules to ensure
#   that we pick up the right module paths
. $SANDBOX/spack/share/spack/setup-env.sh
# load ssg and bake because they are needed by things compiled outside of
# spack later in this script
spack load -r ssg
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
spack load -r bake@dev-file-backend
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
cp $PREFIX/bin/bake-p2p-bw $JOBDIR
cp ${PREFIX}-file/bin/bake-p2p-bw $JOBDIR/bake-p2p-bw-file
cp $PREFIX/bin/pmdk-bw $JOBDIR
# cp $PREFIX/bin/mercury-runner $JOBDIR
cd $JOBDIR

# note: previously we also set --env LD_LIBRARY_PATH=$PREFIX/lib, hopefully no longer needed
JOBID=`qsub --env SANDBOX=$SANDBOX ./margo-regression.qsub`
cqwait $JOBID
JOBID2=`qsub --env SANDBOX=$SANDBOX ./bake-regression.qsub`
cqwait $JOBID2
JOBID3=`qsub --env SANDBOX=$SANDBOX ./pmdk-regression.qsub`
cqwait $JOBID3
# TODO: temporarily disabled
# JOBID4=`qsub --env SANDBOX=$SANDBOX ./mobject-regression.qsub`
# cqwait $JOBID4
JOBID5=`qsub --env SANDBOX=$SANDBOX ./bake-kove.qsub`
cqwait $JOBID5

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
#cat $JOBID.* $JOBID2.* $JOBID3.* $JOBID4.* $JOBID5.* > combined.$JOBID.txt
cat $JOBID.* $JOBID2.* $JOBID3.* $JOBID5.* > combined.$JOBID.txt
dos2unix combined.$JOBID.txt
mailx -s "mochi-regression (cooley)" sds-commits@lists.mcs.anl.gov < combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX
rm -rf $PREFIX

#!/bin/bash

# This is a shell script to be run from a login node of the Theta system at
# the ALCF, that will download, compile, and execute the ssg performance 
# regression tests, including any dependencies

# SEE README.spack.md for environment setup information!  This script will not
#     work properly without properly configured spack environment

# exit on any error
set -e

module swap PrgEnv-intel PrgEnv-gnu
module load cce

export CFLAGS="-O3"
export CRAYPE_LINK_TYPE=dynamic

SANDBOX=$PWD/mochi-regression-sandbox-$$
PREFIX=$PWD/mochi-regression-install-$$
JOBDIR=$PWD/mochi-regression-job-$$

# scratch area to clone and build things
mkdir -p $SANDBOX
cp packages.yaml  $SANDBOX/
cp spack-libfabric-1.7.0.patch $SANDBOX/
cp spack-mercury-udreg-variant.patch $SANDBOX/

# scratch area for job submission
mkdir -p $JOBDIR
cp margo-regression.qsub $JOBDIR
cp bake-regression.qsub $JOBDIR
cp pmdk-regression.qsub $JOBDIR

cd $SANDBOX
git clone https://github.com/spack/spack.git
git clone https://xgitlab.cels.anl.gov/sds/sds-repo.git
git clone https://xgitlab.cels.anl.gov/sds/sds-tests.git
wget http://mvapich.cse.ohio-state.edu/download/mvapich/osu-micro-benchmarks-5.3.2.tar.gz
tar -xvzf osu-micro-benchmarks-5.3.2.tar.gz
git clone https://github.com/pdlfs/mercury-runner.git

# set up most of the libraries in spack
echo "=== BUILD SPACK PACKAGES AND LOAD ==="
cd $SANDBOX/spack
. $SANDBOX/spack/share/spack/setup-env.sh
spack compiler find
spack compilers
# put packages file in place in SPACK_ROOT to set our preferences for building
# Mochi stack
cp $SANDBOX/packages.yaml $SPACK_ROOT/etc/spack
# set up repos file to point to sds-repo; we do this manually because 
#    "spack repo add" will create files in ~/.spack, which is a bad idea in 
#    CI environments
echo "repos:" > $SPACK_ROOT/etc/spack/repos.yaml
echo "- ${SANDBOX}/sds-repo" >> $SPACK_ROOT/etc/spack/repos.yaml
spack uninstall -R -y argobots mercury libfabric | true
spack install ssg
spack install bake
# deliberately repeat setup-env step after building modules to ensure
#   that we pick up the right module paths
. $SANDBOX/spack/share/spack/setup-env.sh
spack load -r ssg
spack load -r bake

# OSU MPI benchmarks
# echo "=== BUILDING OSU MICRO BENCHMARKS ==="
# cd $SANDBOX/osu-micro-benchmarks-5.3.2
# mkdir build
# cd build
# ../configure --prefix=$PREFIX CC=mpicc CXX=mpicxx
# make -j 3
# make install

# sds-tests
echo "=== BUILDING SDS TEST PROGRAMS ==="
cd $SANDBOX/sds-tests
libtoolize
./prepare.sh
mkdir build
cd build
../configure --prefix=$PREFIX CC=cc
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
cp $PREFIX/bin/pmdk-bw $JOBDIR
# cp $PREFIX/libexec/osu-micro-benchmarks/mpi/pt2pt/osu_latency $JOBDIR
# cp $PREFIX/bin/mercury-runner $JOBDIR
cd $JOBDIR
JOBID=`qsub --env SANDBOX=$SANDBOX ./margo-regression.qsub`
cqwait $JOBID
JOBID2=`qsub --env SANDBOX=$SANDBOX ./bake-regression.qsub`
cqwait $JOBID2
JOBID3=`qsub --env SANDBOX=$SANDBOX ./pmdk-regression.qsub`
cqwait $JOBID3

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat $JOBID.* $JOBID2.* $JOBID3.* > combined.$JOBID.txt
#dos2unix combined.$JOBID.txt
mailx -r carns@mcs.anl.gov -s "mochi-regression (theta)" sds-commits@lists.mcs.anl.gov < combined.$JOBID.txt
cat combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX
rm -rf $PREFIX

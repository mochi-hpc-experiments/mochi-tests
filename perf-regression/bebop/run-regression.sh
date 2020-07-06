#!/bin/bash

# This is a shell script to be run from a login node of the Bebop system at
# the LCRC, that will download, compile, and execute the ssg performance 
# regression tests, including any dependencies

# SEE README.spack.md for environment setup information!  This script will not
#     work properly without properly configured spack environment

# exit on any error
set -e

# load newer gcc up front
module load gcc_new/7.3.0

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
cp $ORIGIN/margo-regression.sbatch $JOBDIR
cp $ORIGIN/margo-vector-regression.sbatch $JOBDIR
cp $ORIGIN/bake-regression.sbatch $JOBDIR
cp $ORIGIN/pmdk-regression.sbatch $JOBDIR
cp $ORIGIN/mobject-regression.sbatch $JOBDIR

# set up build environment
cd $SANDBOX
git clone -q https://github.com/spack/spack.git
(cd spack && git checkout -b spack-0.15.0 v0.15.0)
# 0.15.0 bugfix: https://github.com/spack/spack/issues/17371 : Intel-MPI vs Intel-Mpi)
# 0.15.0 bugfix:   plus a fix data structure changed in 0.15 )
(cd spack && git cherry-pick e9e3e88f637dcaa8e3d7a463c6af03ce77c49ce1 &&
             git cherry-pick bb15addad5226fd64a4bfe824669376e270e9db8)
git clone -q https://xgitlab.cels.anl.gov/sds/sds-repo.git
git clone -q https://xgitlab.cels.anl.gov/sds/sds-tests.git

echo "=== BUILD SPACK PACKAGES AND LOAD ==="
. $SANDBOX/spack/share/spack/setup-env.sh
spack compiler find
spack compilers

# use our own packages.yaml for bebop-specific preferences
cp $ORIGIN/packages.yaml $SPACK_ROOT/etc/spack
# add external repo for mochi.  Note that this will not modify the 
# user's ~/.spack/ files because we modified $HOME above
spack repo add ${SANDBOX}/sds-repo
# sanity check
spack repo list
# clean out any stray packages from previous runs, just in case
spack uninstall -R -y argobots mercury opa-psm2 mochi-bake || true

# nightly tests should test nightly software!
# spack install ior@master+mobject ^margo@develop ^mercury@develop ^mobject@develop ^bake@develop ^remi@develop ^thallium@develop ^sdskeyval@develop ^ssg@develop

# ior acts as our "apex" package here, causing several other packages to build
spack install ior@master +mobject

spack install mochi-bake
spack install mochi-ssg

# deliberately repeat setup-env step after building modules to ensure
#   that we pick up the right module paths
. $SANDBOX/spack/share/spack/setup-env.sh
# load ssg and bake because they are needed by things compiled outside of
# spack later in this script
spack load -r mochi-ssg
spack load -r mochi-bake

# sds-tests
echo "=== BUILDING SDS TEST PROGRAMS ==="
cd $SANDBOX/sds-tests
./prepare.sh
mkdir build
cd build
echo ../configure --prefix=$PREFIX CC=mpicc 
../configure --prefix=$PREFIX CC=mpicc
make -j 3
make install

# set up job to run
echo "=== SUBMITTING AND WAITING FOR JOB ==="
cp $PREFIX/bin/margo-p2p-latency $JOBDIR
cp $PREFIX/bin/margo-p2p-bw $JOBDIR
cp $PREFIX/bin/margo-p2p-vector $JOBDIR
cp $PREFIX/bin/bake-p2p-bw $JOBDIR
cp $PREFIX/bin/pmdk-bw $JOBDIR
cd $JOBDIR

export SANDBOX
sbatch --wait --export=ALL ./margo-regression.sbatch || true
sbatch --wait --export=ALL ./bake-regression.sbatch || true
sbatch --wait --export=ALL ./pmdk-regression.sbatch || true
sbatch --wait --export=ALL ./mobject-regression.sbatch || true
sbatch --wait --export=ALL ./margo-vector-regression.sbatch || true

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat *.out | col -b > combined.$SLURM_JOB_ID.txt
# dos2unix combined.$SLURM_JOB_ID.txt
mailx -s "mochi-regression (bebop)" sds-commits@lists.mcs.anl.gov < combined.$SLURM_JOB_ID.txt

cd /tmp
rm -rf $SANDBOX
rm -rf $PREFIX

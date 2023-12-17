#!/bin/bash

# This is a shell script to be run from a login node of the Polaris system at
# the ALCF, that will download, compile, and execute Mochi performance
# regression tests

# exit on any error
set -e

# location of this script
ORIGIN=$(realpath $(dirname "$0"))
# install area; based on pid if this script is executed by hand
SANDBOX=$ORIGIN/sandbox-$$

./run-regression-build-stage.sh $SANDBOX

echo "=== SUBMIT AND WAIT FOR JOBS ==="
cd $SANDBOX/install/bin
export SANDBOX

# continue on these even if a job fails so that we get emails with partial
# results
qsub -W block=true -v SANDBOX -o gpu.out -e gpu.err ./run_gpu_margo_p2p_bw.qsub || true
qsub -W block=true -v SANDBOX -o margo.out -e margo.err ./margo-regression.qsub || true
qsub -W block=true -v SANDBOX -o vector.out -e vector.err ./margo-vector-regression.qsub || true

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
# gather output, strip out funny characters, mail
cat margo.err margo.out margo-regression.output vector.err vector.out gpu.err gpu.out > combined.$JOBID.txt
dos2unix combined.$JOBID.txt
mailx -s "mochi-regression (polaris)" sds-commits@lists.mcs.anl.gov < combined.$JOBID.txt
cat combined.$JOBID.txt

cd /tmp
rm -rf $SANDBOX


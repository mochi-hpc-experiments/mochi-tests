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
qsub -W block=true -v SANDBOX -o margo.out -e margo.err ./margo-regression.qsub || true
qsub -W block=true -v SANDBOX -o gpu.out -e gpu.err ./margo-gpu-regression.qsub || true
qsub -W block=true -v SANDBOX -o vector.out -e vector.err ./margo-vector-regression.qsub || true

echo "=== JOB DONE, COLLECTING AND SENDING RESULTS ==="
cat margo-regression.output margo-gpu-regression.output margo-vector-regression.output > all.output
cat all.output
mailx -s "mochi-regression (Polaris, interactive run)" sds-commits@lists.mcs.anl.gov < all.output

cd /tmp
rm -rf $SANDBOX


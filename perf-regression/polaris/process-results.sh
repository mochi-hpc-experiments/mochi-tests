#!/usr/bin/env bash

today=$(date +%Y-%m-%d)

echo "==> Cloning mochi-hpc-experiments.github.io repository"
git clone https://github.com/mochi-hpc-experiments/mochi-hpc-experiments.github.io.git
POLARIS_RESULT_PATH=mochi-hpc-experiments.github.io/src/supercomputers/polaris

echo "==> Validating results in margo-regression.output"
if (( $(grep -c "## Margo CXI" margo-regression.output) != 14 )); then
    echo "margo-regression.output does not seem to be valid, skipping."
else
    if [[ ! -f $POLARIS_RESULT_PATH/latency.csv ]]; then
        echo "date,op,iterations,warmup_iterations,size,min>,q1,med,avg,q3,max,busy_spin" \
            > $POLARIS_RESULT_PATH/latency.csv
    fi
    if [[ ! -f $POLARIS_RESULT_PATH/bandwidth.csv ]]; then
        echo "date,op,warmup_seconds,concurrency,threads,xfer_size,total_bytes,seconds,MiB/s,xfer_memory,align_buffer,busy_spin" \
            > $POLARIS_RESULT_PATH/bandwidth.csv
    fi
    noop_count=0
    busy_spin=false
    echo "==> Processing margo-regression.output"
    while read -r line; do
        if [[ "$line" = noop* ]]; then
            # Note: the extra "true/false" is for busy spin.
            if (( noop_count == 1 )); then
                busy_spin=true
            fi
            # Convert to comma-separated values
            echo "${today},${line//$'\t'/,},${busy_spin}" >> $POLARIS_RESULT_PATH/latency.csv
            ((noop_count++))
        elif [[ "$line" = PULL* ]] || [[ "$line" = PUSH* ]]; then
            # Convert to comma-separated values
            echo "${today},${line//$'\t'/,},${busy_spin}" >> $POLARIS_RESULT_PATH/bandwidth.csv
        fi
    done < margo-regression.output
    echo "==> Adding files to next commit"
    pushd $POLARIS_RESULT_PATH
    git add latency.csv
    git add bandwidth.csv
    popd
fi

echo "==> Validating results in margo-gpu-regression.output"
if (( $(grep -c -e "^PUSH" -e "^PULL" margo-gpu-regression.output) != 8 )); then
    echo "margo-gpu-regression.output does not seem to be valid, skipping"
else
    if [[ ! -f $POLARIS_RESULT_PATH/gpu-bandwidth.csv ]]; then
        echo "date,op,warmup_seconds,concurrency,threads,xfer_size,total_bytes,seconds,MiB/s,xfer_memory,align_buffer,hostA_mem,hostB_mem" \
            > $POLARIS_RESULT_PATH/gpu-bandwidth.csv
    fi
    echo "==> Processing margo-gpu-regression.output"
    test_num=0
    while read -r line; do
        if [[ "$line" = "=== Testing"* ]]; then
            case "$test_num" in
                0) hostA_mem=GPU; hostB_mem=GPU ;;
                1) hostA_mem=GPU; hostB_mem=CPU ;;
                2) hostA_mem=CPU; hostB_mem=GPU ;;
                3) hostA_mem=CPU; hostB_mem=CPU ;;
            esac
            (( test_num++ ))
        fi
        if [[ "$line" = PULL* ]]; then
            # Convert to comma-separated values
            echo "${today},${line//$'\t'/,},${hostA_mem},${hostB_mem}" >> $POLARIS_RESULT_PATH/gpu-bandwidth.csv
        fi
    done < margo-gpu-regression.output
    echo "==> Adding files to next commit"
    pushd $POLARIS_RESULT_PATH
    git add gpu-bandwidth.csv
    popd
fi

echo "==> Copying spack files"
cp spack.* $POLARIS_RESULT_PATH
pushd mochi-hpc-experiments.github.io
git add src/supercomputers/polaris/spack.*
popd

pushd mochi-hpc-experiments.github.io
echo "==> Creating commit"
git commit -m "[ALCF GitLab CI] Results for ${today}"
echo "==> Changing repository URL"
# See note below for token generation
git remote set-url origin \
    "https://$MOCHI_CI_GH_POLARIS@github.com/mochi-hpc-experiments/mochi-hpc-experiments.github.io.git"
echo "==> Pushing updates"
git push -u origin main
popd

# MOCHI_CI_GH_POLARIS is a token generated on Github that is allowed to read/write
# the mochi-hpc-experiments/mochi-hpc-experiments.github.io repository.
# It has an expiration date. If it needs to be renewed, the following must be done:
#
# - Navigate to https://github.com/settings/personal-access-tokens
# - Click on the token and click "Regenerate token"
# - Copy the token
# - Navitage to https://gitlab-ci.alcf.anl.gov/anl/radix-io/mochi-regression-polaris/-/settings/ci_cd
# - In the "Variables" section click the "Edit" button of the MOCHI_CI_GH_POLARIS variable
# - Past the copied token and click "Save changes"
#
# If the person managing this CI is no longer Matthieu Dorier, you may need a combination
# of the above and the below instructions (which is how the token was created in the first place).
#
# - Navigate to https://github.com/settings/personal-access-tokens
# - Click "Generate New Token"
# - Give it the name MOCHI_CI_GH_POLARIS
# - Resource owner should be set to "mochi-hpc-experiments"
# - Expiration should be set to 365 days
# - Repository access should be set to "Only selected repositories", and select
#   mochi-hpc-experiments/mochi-hpc-experiments.github.io
# - Add permissions "Metadata" and "Contents", and select "Access: Read and Write" for the latter
# - Click Generate token, and copy the token
# - Navigate to https://gitlab-ci.alcf.anl.gov/anl/radix-io/mochi-regression-polaris/-/settings/ci_cd
# - In the "Variables" section click "Add variable"
# - Enter MOCHI_CI_GH_POLARIS as key and the copied token as value, then click "Add variable"

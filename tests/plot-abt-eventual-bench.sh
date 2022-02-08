#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: plot-abt-eventual-bench.sh <data file>"
    exit 1
fi

DAT=$1

# stack exchange says I can do this to find the location of the script
# itself, which is where the gnuplot scripts are that we want to use
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

gnuplot -c $SCRIPT_DIR/rate.plt $DAT $DAT.rate.png
gnuplot -c $SCRIPT_DIR/cpu.plt $DAT $DAT.cpu.png


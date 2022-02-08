#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: sweep-abt-eventual-bench.sh <executable>"
    exit 1
fi

EXE=$1
OUTFILE=sweep-abt-eventual-bench.$$.dat

echo writing results to $OUTFILE

# sweep through number of ULTs in increments of 100 from 100 to 1000
for i in `seq 100 100 1000`; do
    echo ... running benchmark with $i ULTs
    $EXE -n $i >> $OUTFILE
done

echo results are in $OUTFILE

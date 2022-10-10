#!/bin/bash

if [ "$#" -eq 0 ]; then
    echo "Usage: sweep-abt-eventual-bench.sh <executable>"
    exit 1
fi

EXE=$@
OUTFILE=sweep-abt-eventual-bench.$$.dat

echo writing results to $OUTFILE

# sweep through number of ULTs in increments of 100 from 250 to 4850 (20 points)
for  i in `seq 100 250 4850`; do
    echo ... running benchmark with $i ULTs
    $EXE -n $i >> $OUTFILE
done

echo results are in $OUTFILE

#set data style lines
#set style data linespoints
set xlabel 'Number of ULTs'
set ylabel 'CPU time (s)'
set term png medium
#set term po eps mono dashed "Helvetica" 16
#set term post eps color "Times-Roman" 20
#set size 1.4,1.4
#set key right bottom
#set pointsize 2
set key off

INFILE=ARG1
OUTFILE=ARG2

set yrange [0:]
set output OUTFILE
plot INFILE using 1:5 with lines

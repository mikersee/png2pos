#!/usr/bin/gnuplot
set terminal png notransparent nointerlace font 'Helvetica' 11 size 900,600 
set output "debug_histogram.png"
set grid
set nokey
set title "Histogram"
set xlabel "hue"
set ylabel "count"
#set xrange [0:255]
set xtics 0,16,255
set autoscale y
#set logscale y
#set style data histogram
#set style fill solid border
plot "debug_histogram.txt" using 2 with lines linewidth 2

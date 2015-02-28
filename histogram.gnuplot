#!/usr/bin/gnuplot

set terminal png notransparent nointerlace font 'Helvetica' 11 size 800,510 
set output 'debug_histogram.png'

set title 'Histogram'
set style data histogram
set style histogram clustered gap 1
set style fill solid

set xlabel 'hue'
set xrange [0:255]
set xtics 0,16,255
set format x '%02x'

set ylabel 'frequency'
set autoscale y
unset ytics

plot 'debug_histogram.txt' using 2 title 'raw', \
	'debug_histogram_pp.txt' using 2 title 'post-processed'

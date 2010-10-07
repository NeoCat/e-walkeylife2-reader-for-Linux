#!/bin/sh
./show-history > log
[ "$1" != "" ] && s=\""$1"\"
[ "$2" != "" ] && e=\""$2"\"
printf "
set terminal x11
set xdata time
set timefmt \"%%Y/%%m/%%d\"
set grid
set format x \"%%Y\\\\n%%m/%%d\"
plot [$s:$e] \"log\" i 1 u 1:4 w boxes fs solid t \"run\", \"log\" i 1 u 1:2 w boxes fs solid t \"walk\"
" | gnuplot -persist -geometry 1024x360

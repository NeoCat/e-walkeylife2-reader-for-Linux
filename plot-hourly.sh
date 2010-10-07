#!/bin/sh
./show-history > log
[ "$1" != "" ] && s=\""$1"\"
[ "$2" != "" ] && e=\""$2"\"
printf "
set terminal x11
set xdata time
set timefmt \"%%Y/%%m/%%d %%H:\"
set grid
set format x \"%%Y\\\\n%%m/%%d\\\\n%%H\"
plot [$s:$e] \"log\" i 0 u 1:5 w boxes fs solid t \"run\", \"log\" i 0 u 1:3 w boxes fs solid t \"walk\"
" | gnuplot -persist -geometry 1024x360

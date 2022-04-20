#!/bin/sh
gnome-terminal -- ./tse
bash cluster1.sh
bash cluster2.sh
# bash cluster3.sh
gnome-terminal -- ./tsc -h 0.0.0.0 -p 3010 -i 3
gnome-terminal -- ./tsc -h 0.0.0.0 -p 3010 -i 4
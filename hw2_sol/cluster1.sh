#!/bin/sh
gnome-terminal -- ./tsd -h 0.0.0.0 -p 3010 -c 3030 -i 1 -t master
gnome-terminal -- ./tsd -h 0.0.0.0 -p 3010 -c 3040 -i 1 -t slave
gnome-terminal -- ./tsf -h 0.0.0.0 -p 3010 -c 3050 -1 1
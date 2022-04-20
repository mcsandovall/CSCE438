#!/bin/sh
gnome-terminal -- ./tsd -h 0.0.0.0 -p 3010 -c 3060 -i 2 -t master
gnome-terminal -- ./tsd -h 0.0.0.0 -p 3010 -c 3070 -i 2 -t slave
gnome-terminal -- ./tsf -h 0.0.0.0 -p 3010 -c 3080 -i 2
#!/bin/sh
gnome-terminal -- ./tsd -h 0.0.0.0 -p 3010 -c 3100 -i 3 -t master
gnome-terminal -- ./tsd -h 0.0.0.0 -p 3010 -c 3120 -i 3 -t slave
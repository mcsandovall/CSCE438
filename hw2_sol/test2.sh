gnome-terminal -- ./tse
gnome-terminal -- ./tsd -h 0.0.0.0 -p 3010 -c 3030 -i 1 -t master
# gnome-terminal -- ./tsd -h 0.0.0.0 -p 3010 -c 3040 -i 1 -t slave
# bash cluster2.sh
# gnome-terminal -- ./tsf -h 0.0.0.0 -p 3010 -c 3040 -i 2
# gnome-terminal -- ./tsf -h 0.0.0.0 -p 3010 -c 3030 -i 1
# gnome-terminal -- ./tsf -h 0.0.0.0 -p 3010 -c 3080 -i 3
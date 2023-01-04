#!/bin/bash

case $1 in
0) echo "Abortando práctica"
    ps axf | grep "./practica" | grep -v grep | awk '{print "kill -9 " $1}' |sh;;
1) echo "Señal SIGUSR1 (Cliente APP)"
    ps axf | grep "./practica" | grep -v grep | awk '{print "kill -10 " $1}' |sh;;
2) echo "Señal SIGUSR2 (Cliente RED)"
    ps axf | grep "./practica" | grep -v grep | awk '{print "kill -12 " $1}' |sh;;
3) echo "Múltiples señales"
    for i in {1..15}
    do
        ps axf | grep "./practica" | grep -v grep | awk '{print "kill -10 " $1}' |sh
        ps axf | grep "./practica" | grep -v grep | awk '{print "kill -12 " $1}' |sh
        sleep 0.5
    done;;

*) echo "Opción no válida";;
esac
#!/bin/bash

case $1 in
    0)
        echo "Abortando práctica"
        ps axf | grep "./practica" | grep -v grep | awk '{print "kill -9 " $1}' |sh
    ;;
    1)
        echo "Señal SIGUSR1 (Cliente APP)"
        ps axf | grep "./practica" | grep -v grep | awk '{print "kill -10 " $1}' |sh
    ;;
    2)
        echo "Señal SIGUSR2 (Cliente RED)"
        ps axf | grep "./practica" | grep -v grep | awk '{print "kill -12 " $1}' |sh
    ;;
    3)
        echo "Múltiples señales SIGUSR1 y SIGUSR2"
        for i in {1..3000}
        do
            ps axf | grep "./practica" | grep -v grep | awk '{print "kill -10 " $1}' |sh
            sleep 0.01
            ps axf | grep "./practica" | grep -v grep | awk '{print "kill -12 " $1}' |sh
            sleep 0.01
        done
    ;;
    4)
        echo "Señal SIGPIPE (Ampliar clientes)"
        ps axf | grep "./practica" | grep -v grep | awk '{print "kill -13 " $1}' |sh
    ;;
    5)
        echo "Señal SIGALARM (Ampliar técnicos)"
        ps axf | grep "./practica" | grep -v grep | awk '{print "kill -14 " $1}' |sh
    ;;
    *)
        echo "Opción no válida"
    ;;
esac
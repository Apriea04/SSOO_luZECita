#!/bin/bash

gcc practica.c -o practica -lpthread

es_numero="^[0-9]+$"

while true
do
    
    echo "¿Qué quieres enviar como parámetro al programa?"
    echo "(1) Número máximo de clientes"
    echo "(2) Número máximo de técnicos"
    echo "(3) Ambos"
    
    read option
    
    case $option in
        1)
            echo "Número máximo de clientes"
            read clientes
            
            if [[ $clientes =~ $es_numero ]]
            then
                ./practica --clientes $clientes
                exit 0
            else
                echo "ERROR: Debes introducir un número"
            fi
        ;;
        
        2)
            echo "Número máximo de técnicos"
            read tecnicos
            
            if [[ $tecnicos =~ $es_numero ]]
            then
                ./practica --tecnicos $tecnicos
                exit 0
            else
                echo "ERROR: Debes introducir un número"
            fi
        ;;
        
        3)
            echo "Número máximo de clientes"
            read clientes
            
            echo "Número máximo de técnicos"
            read tecnicos
            
            if [[ $clientes =~ $es_numero ]] || [[ $tecnicos =~ $es_numero ]]
            then
                ./practica --clientes $clientes --tecnicos $tecnicos
                exit 0
            else
                echo "ERROR: Debes introducir un número"
            fi
        ;;
        
        *)echo "ERROR: No puede añadir más de 2 argumentos";;
    esac
done

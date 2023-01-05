#!/bin/bash

gcc practica.c -o practica -lpthread

es_numero="^[0-9]+$"

while true
do

echo "¿Cuántos argumentos desea enviar: 1 o 2?"

read option

case $option in
1) 
echo "Número máximo de clientes"
read clientes

if [[ $clientes =~ $es_numero ]]
then
./practica $clientes
exit 0
else
echo "ERROR: Debes introducir un número"
fi
;;

2) 
echo "Número máximo de clientes"
read clientes

echo "Número máximo de técnicos"
read tecnicos

if [[ $clientes =~ $es_numero ]] || [[ $tecnicos =~ $es_numero ]]
then
./practica $clientes $tecnicos
exit 0
else
echo "ERROR: Debes introducir un número"
fi
;;

*)echo "ERROR: No puede añadir más de 2 argumentos";;
esac
done

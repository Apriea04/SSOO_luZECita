# SISTEMA DE GESTIÓN DE AVERÍAS luCEZita
Práctica final de la asignatura de Sistemas Operativos.
## Autores
* Álvaro Prieto Álvarez (apriea04).
* Daniel Martínez Sánchez (dmarts05).
* Guillermo Martínez Martínez (gmartm08).
* Mario López Barazón (mlopeb04).
## Instrucciones para ejecutar la práctica
La práctica se puede ejecutar de dos posibles maneras:
### Manualmente
1. Compilar la práctica: `gcc practica.c -o practica -lpthread`.
2. Ejecutar la práctica:  `./practica` (valores por defecto) o `./practica --clientes $numClientes --tecnicos $numTecnicos`.
### Mediante Makefile
* `make` para ejecutar la práctica con los valores por defecto.
* `make args` para ejecutar la práctica con parámetros de clientes y/o técnicos distintos (abre un script de bash).
## Envío de señales
Para enviar señales al programa existen dos opciones:
### Manualmente
* `kill -10 $pidPractica` para enviar un cliente de tipo APP.
* `kill -12 $pidPractica` para enviar un cliente de tipo RED.
* `kill -13 $pidPractica` para ampliar el número de clientes que soporta la aplicación durante la ejecución.
* `kill -14 $pidPractica` para ampliar el número de técnicos durante la ejecución.
### Mediante Makefile
Esta opción detecta automáticamente el pid del programa.
* `make sigusr1` para enviar un cliente de tipo APP.
* `make sigusr2` para enviar un cliente de tipo RED.
* `make sigpipe` para ampliar el número de clientes que soporta la aplicación durante la ejecución.
* `make sigalarm` para ampliar el número de técnicos durante la ejecución.
* `make stress` para enviar una gran cantidad de clientes de distintos tipos al programa.

exec:	compile
	./practica

compile:
	gcc practica.c -o practica -lpthread

sig1:
	sh signals.sh 1
sig2:
	sh signals.sh 2
stress:
	sh signals.sh 3
kill:
	sh signals.sh 0
args:
	sh argumentos.sh

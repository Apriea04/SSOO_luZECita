exec:	compile
	./practica

compile:
	gcc practica.c -o practica -lpthread

sigusr1:
	sh signals.sh 1
sigusr2:
	sh signals.sh 2
sigpipe:
	sh signals.sh 4
sigalarm:
	sh signals.sh 5
stress:
	sh signals.sh 3
kill:
	sh signals.sh 0
args:
	make compile
	sh argumentos.sh

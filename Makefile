exec:	compile
	./practica

compile:
	gcc practica.c -o practica -lpthread

sigusr1:
	bash signals.sh 1
sigusr2:
	bash signals.sh 2
sigpipe:
	bash signals.sh 4
sigalarm:
	bash signals.sh 5
stress:
	bash signals.sh 3
kill:
	bash signals.sh 0
args:
	make compile
	bash argumentos.sh

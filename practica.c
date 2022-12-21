#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define numClientes 20


/**DECLARACIONES GLOBALES*/

//Falta declarar las variables condición

pthread_mutex_t Fichero, colaClientes, solicitudes;

int contadorApp, contadorRed;

int numSolicDomicilio;

struct Clientes {
	int id;
	int atendido; 	//0 SI ATENDIDO; 1 SI ESTÁ EN PROCESO; 2 SI ESTÁ ATENDIDO
	int tipo;		// 0 SI APP; 1 SI RED
	int prioridad;
	int solicitud;

};


int tecnico[2];		//REVISAR, NO ESTÁ BIEN

FILE *logFile;


/**FUNCIONES*/

void writeLogMessage(char *id, char *msg) {
	//	Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[25];
	strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);

	// Escribimos en el log

	logFile = fopen("registroTiempos.log", "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);
}

/**MANEJADORAS DE SEÑAL*/

void handlerClienteApp(int sig){
	printf("Hola SIGUSR1\n");
}

void handlerClienteRed(int sig){
	printf("Hola SIGUSR2\n");
}

void handlerTerminar(int sig){
	printf("Hola SIGINT\n");
	exit(0);		//ESTO SOBRA, ES PARA SALIR POR AHORA
}

/**CÓDIGOS DE EJECUCIÓN DE HILOS*/

void *Tecnico(void *arg) {
	printf("Técnico %d\n", (int*) arg);
}

void *Responsable(void *arg) {
	printf("Responsable %d\n", (int*) arg);
}

void *Encargado(void *arg) {
	printf("Encargado\n");
}

void *AtencionDomiciliaria(void *arg) {
	printf("Atención domiciliaria\n");
}



/**MAIN*/

int main(){
	struct sigaction sig;
	int i;
	sig.sa_handler=handlerClienteApp;
	if(sigaction(SIGUSR1, &sig, NULL)==-1){
		perror("[ERROR] Error en la llamada a sigaction");
		exit(-1);
	}

	sig.sa_handler=handlerClienteRed;
	if(sigaction(SIGUSR2, &sig, NULL)==-1){
		perror("[ERROR] Error en la llamada a sigaction");
		exit(-1);
	}

	sig.sa_handler=handlerTerminar;
	if(sigaction(SIGINT, &sig, NULL)==-1){
		perror("[ERROR] Error en la llamada a sigaction");
		exit(-1);
	}

	/**INICIALIZAR RECURSOS*/
	pthread_mutex_init(&Fichero, NULL);
	pthread_mutex_init(&colaClientes, NULL);
	pthread_mutex_init(&solicitudes, NULL);

	contadorApp = 0;
	contadorRed = 0;

	struct Clientes listaclientes[numClientes];
	for(i=0; i<numClientes;i++){
		listaclientes[i].id = 0;
		listaclientes[i].prioridad = 0;
		listaclientes[i].atendido = 0;
		listaclientes[i].solicitud = 0;
	}

	tecnico[0] = 0;
	tecnico[1] = 0;

	pthread_t tecnico_1, tecnico_2, resprep_1, resprep_2, encargado, attdomicil;

	pthread_create(&tecnico_1, NULL, Tecnico, (int*)1);
	pthread_create(&tecnico_2, NULL, Tecnico, (int*)2);
	pthread_create(&resprep_1, NULL, Responsable, (int*) 1);
	pthread_create(&resprep_2, NULL, Responsable, (int*) 2);
	pthread_create(&encargado, NULL, Encargado, NULL);
	pthread_create(&attdomicil, NULL, AtencionDomiciliaria, NULL);
	


	while(1){
		pause();
	}
		
	return 0;
}


//GUILLERMO
void nuevoCliente(){

}


//ÁLVARO
void accionesCliente(){

}

//DANIEL
void accionesTecnico(){

}


//MARIO
void accionesEncargado(){

}


void accionesTecnicoDomiciliario(){

}


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define numClientes 20
#define numTecnicos 2
#define numRespReparaciones 2
#define numEncargados 1
#define numTecAttDomiciliaria 1

/**DECLARACIONES GLOBALES*/

// Mutex
pthread_mutex_t Fichero, colaClientes, solicitudes;

// Variables condición
pthread_cond_t condSolicitudesDomicilio;

// Contadores
int contadorApp, contadorRed, numSolicitudesDomicilio;

struct Clientes
{
	int id;
	int atendido; // 0 SI ATENDIDO; 1 SI ESTÁ EN PROCESO; 2 SI ESTÁ ATENDIDO
	int tipo;	  // 0 SI APP; 1 SI RED
	int prioridad;
	int solicitud;
};

// Listas de hilos trabajadores
pthread_t tecnicos[numTecnicos];
pthread_t respReparaciones[numRespReparaciones];
pthread_t encargados[numEncargados];
pthread_t tecAttDomiciliaria[numTecAttDomiciliaria];

FILE *logFile;

/**FUNCIONES*/

void writeLogMessage(char *id, char *msg)
{
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

void handlerClienteApp(int sig)
{
	printf("Hola SIGUSR1\n");
}

void handlerClienteRed(int sig)
{
	printf("Hola SIGUSR2\n");
}

void handlerTerminar(int sig)
{
	printf("Hola SIGINT\n");
	exit(0); // ESTO SOBRA, ES PARA SALIR POR AHORA
}

/**CÓDIGOS DE EJECUCIÓN DE HILOS*/

void *Tecnico(void *arg)
{
	int index = *(int *)arg;
	printf("Técnico %d\n", index);
	free(arg);
}

void *Responsable(void *arg)
{
	int index = *(int *)arg;
	printf("Responsable %d\n", index);
	free(arg);
}

void *Encargado(void *arg)
{
	int index = *(int *)arg;
	printf("Encargado %d\n", index);
	free(arg);
}

void *AtencionDomiciliaria(void *arg)
{
	int index = *(int *)arg;
	printf("Atención domiciliaria %d\n", index);
	free(arg);
}

/**MAIN*/

int main()
{
	struct sigaction sig;
	int i;
	sig.sa_handler = handlerClienteApp;
	if (sigaction(SIGUSR1, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}

	sig.sa_handler = handlerClienteRed;
	if (sigaction(SIGUSR2, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}

	sig.sa_handler = handlerTerminar;
	if (sigaction(SIGINT, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}

	/**INICIALIZAR RECURSOS*/

	// Inicicialización de mutex
	pthread_mutex_init(&Fichero, NULL);
	pthread_mutex_init(&colaClientes, NULL);
	pthread_mutex_init(&solicitudes, NULL);

	// Inicialización de variables condición
	pthread_cond_init(&condSolicitudesDomicilio, NULL);

	// Inicialización de contadores
	contadorApp = 0;
	contadorRed = 0;
	numSolicitudesDomicilio = 0;

	struct Clientes listaClientes[numClientes];
	for (i = 0; i < numClientes; i++)
	{
		listaClientes[i].id = 0;
		listaClientes[i].prioridad = 0;
		listaClientes[i].atendido = 0;
		listaClientes[i].solicitud = 0;
	}

	// Inicialización de técnicos
	for (i = 0; i < numTecnicos; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&tecnicos[i], NULL, &Tecnico, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de técnico.");
			return -1;
		}
	}

	// Inicialización de responsables de reparaciones
	for (i = 0; i < numRespReparaciones; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&respReparaciones[i], NULL, &Responsable, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de responsable de reparaciones.");
			return -1;
		}
	}

	// Inicialización de encargados
	for (i = 0; i < numEncargados; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&encargados[i], NULL, &Encargado, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de encargado.");
			return -1;
		}
	}

	// Inicialización de técnicos de atención domiciliaria
	for (i = 0; i < numTecAttDomiciliaria; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&tecAttDomiciliaria[i], NULL, &AtencionDomiciliaria, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de técnico de atención domiciliaria.");
			return -1;
		}
	}

	while (1)
	{
		pause();
	}

	// IMPORTANTE: No sé si esto es necesario, conviene revisarlo posteriormente - Joins de todos los hilos
	// for (i = 0; i < numTecnicos; i++)
	// {
	// 	if (pthread_join(tecnicos[i], NULL) != 0)
	// 	{
	// 		perror("[ERROR] Error al unir hilo de técnico.");
	// 		return -1;
	// 	}
	// }

	// for (i = 0; i < numRespReparaciones; i++)
	// {
	// 	if (pthread_join(respReparaciones[i], NULL) != 0)
	// 	{
	// 		perror("[ERROR] Error al unir hilo de responsable de reparaciones.");
	// 		return -1;
	// 	}
	// }

	// for (i = 0; i < numEncargados; i++)
	// {
	// 	if (pthread_join(encargados[i], NULL) != 0)
	// 	{
	// 		perror("[ERROR] Error al unir hilo de encargado.");
	// 		return -1;
	// 	}
	// }

	// for (i = 0; i < numTecAttDomiciliaria; i++)
	// {
	// 	if (pthread_join(tecAttDomiciliaria[i], NULL) != 0)
	// 	{
	// 		perror("[ERROR] Error al unir hilo de técnico de atención domiciliaria.");
	// 		return -1;
	// 	}
	// }

	// Eliminar los mutex al salir
	pthread_mutex_destroy(&Fichero);
	pthread_mutex_destroy(&colaClientes);
	pthread_mutex_destroy(&solicitudes);

	return 0;
}

// GUILLERMO
void nuevoCliente()
{
}

// ÁLVARO
void accionesCliente()
{
}

// DANIEL
void accionesTecnico()
{
}

// MARIO
void accionesEncargado()
{
}

void accionesTecnicoDomiciliario()
{
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#define NUMRESPREPARACIONES 2  // Número de responsables de reparaciones
#define NUMENCARGADOS 1		   // Número de encargados
#define NUMTECDOMICILIARIO 1   // Número de técnicos de atención domiciliaria
#define NUMSOLDOMINECESARIAS 4 // Número de solicitudes hasta que el técnico domiciliario atienda a los clientes

/**
 * @author mlopeb04
 * @author gmartm08
 * @author dmarts05
 * @author apriea04
 *
 */

/**DECLARACIONES GLOBALES*/

// Cantidad de trabajadores y clientes
int numClientes;
int numTecnicos;

// Mutex
pthread_mutex_t Fichero, mutexColaClientes, mutexTecnicos, mutexRespReparaciones, mutexSolicitudesDomicilio, mutexViaje;

// Variables condición
pthread_cond_t condSolicitudesDomicilio;

// Contadores
int contadorApp, contadorRed, numSolicitudesDomicilio;

// Variable para la salida controlada
int finalizar, ordenarAcabar;

// Sigaction
struct sigaction sig;

// Struct que define a un cliente
struct Cliente
{
	int id;					// Número secuencial comenzando en 1 para su tipo de cliente
	int atendido;			// 0 si no atendido; 1 si está en proceso; 2 si ya está atendido; 3 si se confunde o está mal identificado
	int tipo;				// 0 si APP; 1 si RED
	int prioridad;			// Del 1 al 10 aleatoria. 10 es la prioridad más alta
	int solicitudDomicilio; // 0 si no solicita atención domiciliaria o ya se le atendió domiciliariamente; 1 si sí la solicita
};

// Struct que define a un trabajador
struct Trabajador
{
	int id;								   // Número secuencial comenzando en 1 para cada trabajador
	int disponible;						   // 0 si no está disponible; 1 si está disponible
	int numClientesAtendidosHastaDescanso; // Número clientes atendidos por el trabajador
};

// Listas de hilos trabajadores
pthread_t *hilosTecnicos;
pthread_t *hilosRespReparaciones;
pthread_t *hilosEncargados;
pthread_t *hilosTecDomiciliarios;

// Lista de clientes
struct Cliente *listaClientes;

// Lista de técnicos
struct Trabajador *listaTecnicos;

// Lista de responsables de reparaciones
struct Trabajador *listaRespReparaciones;

// Log
FILE *logFile;

/**DECLARACIÓN DE FUNCIONES PRINCIPALES*/

// GUILLERMO
/**
 * Introduce un cliente en la cola de clientes cuando se recibe una señal (SIGUSR1 o SIGUSR2).
 *
 * Cuando aparece un cliente, lo introduce en la primera posición libre que encuentre (cuando  id==0).
 *
 * Si no hay posiciones libres, no se hace nada y se ignora el cliente.
 *
 * @param tipoCliente indica si un cliente es de tipo APP o RED (0 o 1 respectivamente).
 */
void nuevoCliente(int tipoCliente);

// ÁLVARO
/**
 * Simula a un cliente que trata de ser atendido en el sistema de gestión de averías.
 *
 * @param posCliente posición del cliente en listaClientes.
 */
void accionesCliente(int posCliente);

// DANIEL
/**
 * Simula las funciones de un técnico o un responsable en el sistema de gestión de averías.
 * Los trabajadores atiende a los clientes de su tipo asignado.
 *
 * Los trabajadores descansan cada cierto número de clientes atendidos.
 *
 * @param tipoTrabajador indica si un trabajador se encarga de clientes tipo APP (técnico) o tipo RED (responsable de reparaciones).
 * @param posTrabajador posición del trabajador en su respectiva lista (listaTecnicos o listaRespReparaciones).
 */
void accionesTecnico(int tipoTrabajador, int posTrabajador);

// MARIO
/**
 * Simula las funciones del encargado en el sistema de gestión de averías.
 *
 * El encargado hace las veces de técnico o responsable de reparaciones cuando estos trabajadores no están disponibles.
 */
void accionesEncargado();

// GRUPO
/**
 * Simula las funciones de un técnico domiciliario en el sistema de gestión de averías.
 *
 * El técnico domiciliario solo comienza a atender cuando hay 4 solicitudes de atención domiciliaria.
 */
void accionesTecnicoDomiciliario();

/**DECLARACIÓN DE FUNCIONES AUXILIARES*/

/**
 * Escribe un mensaje nuevo en el fichero "registroTiempos.log".
 *
 * El mensaje estará compuesto por la fecha y la hora, el identificador del hilo que lo ejecuta y el mensaje.
 *
 * @param id puntero que contiene la cadena de caracteres con el identificador de un hilo.
 * @param msg puntero que contiene la cadena de caracteres con el mensaje a escribir en el log.
 */
void writeLogMessage(char *id, char *msg);

/**
 * Calcula un número aleatorio entre el mínimo y el máximo espeficicados.
 *
 * Ambos números se incluyen en el cálculo.
 *
 * @param min mínimo valor del intervalo.
 * @param max máximo valor del intervalo.
 */
int calculaAleatorios(int min, int max);

/**
 * Libera el cliente de la lista de clientes.
 *
 * @param posCliente posición del cliente en listaClientes.
 */
void liberaCliente(int posicion);

/**
 * Obtiene al próximo cliente que debe ser atendido,
 * teniendo en cuenta su tipo, prioridad y tiempo esperado.
 */
int obtenerPosicionProximoCliente();

/**
 * Obtiene al próximo cliente de un tipo específico que debe
 * ser atendido teniendo en cuenta su prioridad y tiempo esperado.
 *
 * @param tipoCliente tipo de cliente que se va a buscar (APP o RED).
 */
int obtenerPosicionProximoClienteSegunTipo(int tipoCliente);

/**
 * Lleva a cabo el proceso de atención de un trabajador (técnico o responsable de reparaciones) a un cliente.
 *
 * @param tipoTrabajador tipo de trabajador que está atendiendo al cliente.
 * @param posTrabajador posición del trabajador en su respectiva lista (listaTecnicos o listaRespReparaciones).
 * @param tipoCliente tipo de cliente (APP o RED).
 * @param posCliente posición del cliente en listaClientes.
 *
 * @return 0 si no se ha atendido a un cliente; 1 si se ha atendido un cliente
 */
int atenderCliente(int tipoTrabajador, int posTrabajador, int tipoCliente, int posCliente);

/**
 * Escribe en el puntero el identificador del próximo cliente que recibió atención domiciliaria y
 * establece la solicitud a 0 del cliente que la solicitó.
 */
void atenderClienteAttDom(char *cadena);

/**
 * Devuelve 0 si no hay técnicos disponibles, en caso contrario, devuelve el número de
 * técnicos disponibles.
 */
int obtenerNumTecnicosDisponibles();

/**
 * Devuelve 0 si no hay responsables de reparaciones disponibles, en caso contrario, devuelve el número de
 * responsables de reparaciones disponibles.
 */
int obtenerNumRespReparacionesDisponibles();

/**
 * Activa la recepción de señales en el programa.
 */
void activarSenales();

/**
 * Bloquea la recepción de señales en el programa.
 */
void desactivarSenales();

/**
 * Imprime el mensaje de bienvenida a la aplicación.
 */
void printWelcome();

/**CÓDIGOS DE EJECUCIÓN DE HILOS*/

/**
 * Código ejecutado por los hilos de técnicos.
 *
 * @param arg puntero del ID del técnico.
 * @return void*
 */
void *Tecnico(void *arg)
{
	char *id;
	id = malloc(sizeof(char) * 30);

	int index = *(int *)arg;
	accionesTecnico(0, index);
	sprintf(id, "tecnico_%d", index + 1);
	pthread_mutex_lock(&Fichero);
	writeLogMessage(id, "Termina su trabajo");
	pthread_mutex_unlock(&Fichero);
	printf("[SISTEMA] Técnico %d ha finalizado su trabajo.\n", index + 1);
	free(id);
	free(arg);
}

/**
 * Código ejecutado por los hilos de responsables de reparaciones.
 *
 * @param arg puntero del ID del responsable de reparaciones.
 * @return void*
 */
void *Responsable(void *arg)
{
	char *id;
	id = malloc(sizeof(char) * 30);

	int index = *(int *)arg;
	accionesTecnico(1, index);
	sprintf(id, "resprep_%d", index + 1);
	pthread_mutex_lock(&Fichero);
	writeLogMessage(id, "Termina su trabajo");
	pthread_mutex_unlock(&Fichero);
	printf("[SISTEMA] Responsable de reparaciones %d ha finalizado su trabajo.\n", index + 1);
	free(id);
	free(arg);
}

/**
 * Código ejecutado por el hilo de encargado.
 *
 * @param arg puntero del ID del encargado.
 * @return void
 */
void *Encargado(void *arg)
{
	int index = *(int *)arg;
	accionesEncargado();
	char *id;
	id = malloc(sizeof(char) * 30);

	sprintf(id, "encargado_%d", index);
	pthread_mutex_lock(&Fichero);
	writeLogMessage(id, "Termina su trabajo");
	pthread_mutex_unlock(&Fichero);
	printf("[SISTEMA] Encargado %d ha finalizado su trabajo.\n", index);
	free(id);
	free(arg);
}

/**
 * Código ejecutado por el técnico de atención domiciliaria.
 * @param arg puntero del ID del técnico de atención domiciliaria.
 * @return void*
 */
void *AtencionDomiciliaria(void *arg)
{
	char *id;
	id = malloc(sizeof(char) * 30);

	int index = *(int *)arg;
	accionesTecnicoDomiciliario();
	pthread_mutex_lock(&Fichero);
	writeLogMessage("tecnico_dom", "Termina su trabajo");
	pthread_mutex_unlock(&Fichero);
	printf("[SISTEMA] Técnico de atención domiciliaria %d ha finalizado su trabajo.\n", index);
	free(id);
	free(arg);
}

/**
 * Código ejecutado por los clientes entrantes al sistema.
 * @param arg puntero del ID del cliente.
 * @return void*
 */
void *Cliente(void *arg)
{
	int index = *(int *)arg;
	accionesCliente(index);
	free(arg);
}

/**MANEJADORAS DE SEÑAL*/

/**
 * Manejadora de señal que crea un nuevo cliente de tipo APP. Responde a la señal SIGUSR1.
 * @param sig
 */
void handlerClienteApp(int sig)
{
	nuevoCliente(0);
}

/**
 * Manejadorea de señal que crea un nuevo cliente de tipo RED. Responde a la señal SIGUSR2.
 * @param sig
 */
void handlerClienteRed(int sig)
{
	nuevoCliente(1);
}

/**
 * Manejadora de señal que finaliza de manera ordenada el programa. Responde a la señal SIGINT.
 * @param s
 */
void handlerTerminar(int s)
{
	printf("[SISTEMA] Iniciando salida controlada.\n");

	// Escribir en el log que se va a hacer una salida controlada
	pthread_mutex_lock(&Fichero);
	writeLogMessage("SISTEMA", "Iniciando salida controlada.");
	pthread_mutex_unlock(&Fichero);

	// Bloquear recepción de señales en el programa
	desactivarSenales();

	// Provoca la salida controlada de los trabajadores (atendiendo a clientes restantes)
	ordenarAcabar = 1;
}

/**
 * Código de la función manejadora que amplía el total de clientes que puede aceptar el sistema
 */
void handlerAmpliaClientes(int s)
{
	int clientesExtra;
	char *clientesExtraStr;
	clientesExtraStr = malloc(sizeof(char) * 15);
	char *msg;
	msg = malloc(sizeof(char) * 50);

	int entradaValida = 0;

	// Desactivar temporalmente la recepción de señales
	desactivarSenales();

	// Pedir número de clientes
	while (entradaValida == 0)
	{
		printf("[SISTEMA] ¿Cuántos clientes quieres añadir al tope?:\n");
		fgets(clientesExtraStr, 15, stdin);

		// Comprobar si entrada contiene letras
		if (sscanf(clientesExtraStr, "%d", &clientesExtra) == 1)
		{
			// Entrada no tiene letras
			clientesExtra = atoi(clientesExtraStr);

			// Comprobar si entrada tiene un número mayor que 0
			if (clientesExtra > 0)
			{
				// Se cumplen todos los checks
				entradaValida = 1;
			}
		}

		if (entradaValida == 0)
		{
			printf("[SISTEMA] Entrada inválida.\n");
		}
	}

	free(clientesExtraStr);

	// Entrada no válida, salimos del método
	if (entradaValida == 0)
	{
		printf("[SISTEMA] Entrada inválida.\n");
		return;
	}

	// Vamos a modificar la cola de clientes. Zona peligrosa (crítica)
	pthread_mutex_lock(&mutexColaClientes);
	numClientes += clientesExtra;
	listaClientes = realloc(listaClientes, sizeof(struct Cliente) * numClientes);
	// 20; 20 < 40
	for (int i = numClientes - clientesExtra; i < numClientes; i++)
	{
		listaClientes[i].id = 0;
		listaClientes[i].atendido = 0;
		listaClientes[i].prioridad = 0;
		listaClientes[i].solicitudDomicilio = 0;
		listaClientes[i].tipo = 0;
	}
	pthread_mutex_unlock(&mutexColaClientes);

	// Reactivar recepción de señales
	activarSenales();

	printf("[SISTEMA] Lista de clientes ampliada %d posiciones.\n", clientesExtra);

	// Escribir en el log la ampliación
	sprintf(msg, "[SISTEMA] Lista de clientes ampliada %d posiciones", clientesExtra);
	pthread_mutex_lock(&Fichero);
	writeLogMessage("SISTEMA", msg);
	pthread_mutex_unlock(&Fichero);

	free(msg);
}

/**
 * Código de la función manejadora que amplía el total de clientes que puede aceptar el sistema
 */
void handlerAmpliaTecnicos(int s)
{
	int tecnicosExtra;
	char *tecnicosExtraStr;
	tecnicosExtraStr = malloc(sizeof(char) * 15);
	char *msg;
	msg = malloc(sizeof(char) * 50);

	int entradaValida = 0;

	// Desactivar temporalmente la recepción de señales
	desactivarSenales();

	// Pedir número de técnicos
	while (entradaValida == 0)
	{
		printf("[SISTEMA] ¿Cuántos técnicos quieres añadir?:\n");
		fgets(tecnicosExtraStr, 15, stdin);

		// Comprobar si entrada contiene letras
		if (sscanf(tecnicosExtraStr, "%d", &tecnicosExtra) == 1)
		{
			// Entrada no tiene letras
			tecnicosExtra = atoi(tecnicosExtraStr);

			// Comprobar si entrada tiene un número mayor que 0
			if (tecnicosExtra > 0)
			{
				// Se cumplen todos los checks
				entradaValida = 1;
			}
		}

		if (entradaValida == 0)
		{
			printf("[SISTEMA] Entrada inválida.\n");
		}
	}

	free(tecnicosExtraStr);

	pthread_mutex_lock(&mutexTecnicos);
	numTecnicos += tecnicosExtra;
	listaTecnicos = realloc(listaTecnicos, sizeof(struct Trabajador) * numTecnicos);
	hilosTecnicos = realloc(hilosTecnicos, sizeof(pthread_t) * numTecnicos);

	// Creamos un nuevo hilo de técnicos tras aumentar el tamaño de la lista
	for (int i = numTecnicos - tecnicosExtra; i < numTecnicos; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i;
		if (pthread_create(&hilosTecnicos[i], NULL, &Tecnico, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de técnico.");
		}

		listaTecnicos[i].disponible = 1; // El técnico empieza ya disponible
		listaTecnicos[i].id = i + 1;
		listaTecnicos[i].numClientesAtendidosHastaDescanso = 0;
	}
	pthread_mutex_unlock(&mutexTecnicos);

	// Reactivar recepción de señales
	activarSenales();

	printf("[SISTEMA] %d técnicos a mayores listos para trabajar.\n", tecnicosExtra);

	// Escribir en el log la ampliación
	sprintf(msg, "%d técnicos a mayores listos para trabajar", tecnicosExtra);
	pthread_mutex_lock(&Fichero);
	writeLogMessage("SISTEMA", msg);
	pthread_mutex_unlock(&Fichero);

	free(msg);
}

/**MAIN*/

int main(int argc, char *argv[])
{
	ordenarAcabar = 0; // Sirve para que cuando no queden clientes por atender y se haya recibido SIGINT se acabe con los trabajadores
	finalizar = 0;
	printWelcome();
	int i;

	// Activar recepción de señales en el programa
	activarSenales();

	/**INICIALIZAR RECURSOS*/

	// Inicialización de cantidades por defecto de técnicos y clientes
	numClientes = 20;
	numTecnicos = 2;

	// Inicialización de hilos
	hilosTecnicos = malloc((sizeof(pthread_t)) * numTecnicos);
	hilosRespReparaciones = malloc((sizeof(pthread_t) * NUMRESPREPARACIONES));
	hilosEncargados = malloc((sizeof(pthread_t) * NUMENCARGADOS));
	hilosTecDomiciliarios = malloc((sizeof(pthread_t) * NUMTECDOMICILIARIO));

	// Inicialización de mutex
	pthread_mutex_init(&Fichero, NULL);
	pthread_mutex_init(&mutexColaClientes, NULL);
	pthread_mutex_init(&mutexTecnicos, NULL);
	pthread_mutex_init(&mutexRespReparaciones, NULL);
	pthread_mutex_init(&mutexSolicitudesDomicilio, NULL);
	pthread_mutex_init(&mutexViaje, NULL);

	// Limpiar log
	pthread_mutex_lock(&Fichero);
	logFile = fopen("registroTiempos.log", "w");
	fclose(logFile);
	pthread_mutex_unlock(&Fichero);

	char *msg;
	msg = malloc(sizeof(char) * 50);
	// Recibir parámetros del programa
	if (argc >= 3 && atoi(argv[2]) > 0)
	{
		// Primer cambio

		// Comprobar si el primer argumento indica si son clientes o técnicos
		if (strcmp(argv[1], "--clientes") == 0)
		{
			// Modificar número de clientes
			numClientes = atoi(argv[2]);

			// Cambiar mensaje del log
			sprintf(msg, "Se ha cambiado el número máximo de clientes a %d.", numClientes);
		}
		else if (strcmp(argv[1], "--tecnicos") == 0)
		{
			// Modificar número de técnicos
			numTecnicos = atoi(argv[2]);

			// Cambiar mensaje del log
			sprintf(msg, "Se ha cambiado el número máximo de técnicos a %d.", numTecnicos);
		}

		// Escribir log con el primer cambio
		pthread_mutex_lock(&Fichero);
		writeLogMessage("SISTEMA", msg);
		pthread_mutex_unlock(&Fichero);

		// Comprobar si hay un segundo cambio
		if (argc == 5 && atoi(argv[4]) > 0)
		{
			// Segundo cambio

			// Comprobar si el tercer argumento indica si son clientes o técnicos
			if (strcmp(argv[3], "--clientes") == 0)
			{
				// Modificar número de clientes
				numClientes = atoi(argv[4]);

				// Cambiar mensaje del log
				sprintf(msg, "Se ha cambiado el número máximo de clientes a %d.", numClientes);
			}
			else if (strcmp(argv[3], "--tecnicos") == 0)
			{
				// Modificar número de técnicos
				numTecnicos = atoi(argv[4]);

				// Cambiar mensaje del log
				sprintf(msg, "Se ha cambiado el número máximo de técnicos a %d.", numTecnicos);
			}

			// Escribir log con el segundo cambio
			pthread_mutex_lock(&Fichero);
			writeLogMessage("SISTEMA", msg);
			pthread_mutex_unlock(&Fichero);
		}
	}
	// Inicialización de hilos
	hilosTecnicos = malloc((sizeof(pthread_t)) * numTecnicos);
	hilosRespReparaciones = malloc((sizeof(pthread_t) * NUMRESPREPARACIONES));
	hilosEncargados = malloc((sizeof(pthread_t) * NUMENCARGADOS));
	hilosTecDomiciliarios = malloc((sizeof(pthread_t) * NUMTECDOMICILIARIO));

	// Inicialización de variables condición
	pthread_cond_init(&condSolicitudesDomicilio, NULL);

	// Inicialización de contadores
	contadorApp = 0;
	contadorRed = 0;
	numSolicitudesDomicilio = 0;

	// Inicialización de lista de clientes
	listaClientes = (struct Cliente *)malloc(numClientes * sizeof(struct Cliente));
	for (i = 0; i < numClientes; i++)
	{
		listaClientes[i].id = 0;
		listaClientes[i].prioridad = 0;
		listaClientes[i].atendido = 0;
		listaClientes[i].solicitudDomicilio = 0;
	}

	// Inicialización de lista de técnicos
	listaTecnicos = (struct Trabajador *)malloc(numTecnicos * sizeof(struct Trabajador));
	for (i = 0; i < numTecnicos; i++)
	{
		listaTecnicos[i].id = 0;
		// Por defecto disponible
		listaTecnicos[i].disponible = 1;
		listaTecnicos[i].numClientesAtendidosHastaDescanso = 0;
	}

	// Inicialización de técnicos
	for (i = 0; i < numTecnicos; i++)
	{
		// Agregar a lista de técnicos
		listaTecnicos[i].id = i + 1;

		int *index = malloc(sizeof(int));
		*index = i;
		if (pthread_create(&hilosTecnicos[i], NULL, &Tecnico, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de técnico.");
			return -1;
		}
	}

	// Inicialización de lista de reponsables de reparaciones.
	listaRespReparaciones = (struct Trabajador *)malloc(NUMRESPREPARACIONES * sizeof(struct Trabajador));
	for (i = 0; i < NUMRESPREPARACIONES; i++)
	{
		listaRespReparaciones[i].id = 0;
		listaRespReparaciones[i].disponible = 1;
		listaRespReparaciones[i].numClientesAtendidosHastaDescanso = 0;
	}

	// Inicialización de responsables de reparaciones.
	for (i = 0; i < NUMRESPREPARACIONES; i++)
	{
		// Agregar a lista de responsables de reparaciones.
		listaRespReparaciones[i].id = i + 1;

		int *index = malloc(sizeof(int));
		*index = i;
		if (pthread_create(&hilosRespReparaciones[i], NULL, &Responsable, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de responsable de reparaciones.");
			return -1;
		}
	}

	// Inicialización de encargados.
	for (i = 0; i < NUMENCARGADOS; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&hilosEncargados[i], NULL, &Encargado, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de encargado.");
			return -1;
		}
	}

	// Inicialización de técnicos de atención domiciliaria.
	for (i = 0; i < NUMTECDOMICILIARIO; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&hilosTecDomiciliarios[i], NULL, &AtencionDomiciliaria, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de técnico de atención domiciliaria.");
			return -1;
		}
	}

	while (ordenarAcabar == 0)
	{
		pause();
	}

	// Se ordenó finalizar.
	int quedanClientes = -1;
	while (quedanClientes != 0)
	{
		i = 0;
		quedanClientes = 0;
		pthread_mutex_lock(&mutexColaClientes);

		while (i < numClientes && quedanClientes == 0)
		{
			if (listaClientes[i].id != 0 && listaClientes[i].solicitudDomicilio != 1)
			{
				quedanClientes = listaClientes[i].id;
			}
			i += 1;
		}
		pthread_mutex_unlock(&mutexColaClientes);

		// Esperamos a que el personal acabe de atender
		sleep(2);
	}

	finalizar = 1;

	// Enviar señal al técnico domiciliario para atender a los clientes restantes
	printf("[SISTEMA] Enviando señal al técnico domiciliario para atender últimos clientes\n");
	pthread_cond_signal(&condSolicitudesDomicilio);

	// Esperar a que todos los hilos terminen sus funciones antes de acabar el programa
	for (i = 0; i < numTecnicos; i++)
	{
		if (pthread_join(hilosTecnicos[i], NULL) != 0)
		{
			perror("[ERROR] Error al unir hilo de técnico.");
			return -1;
		}
	}

	for (i = 0; i < NUMRESPREPARACIONES; i++)
	{
		if (pthread_join(hilosRespReparaciones[i], NULL) != 0)
		{
			perror("[ERROR] Error al unir hilo de responsable de reparaciones.");
			return -1;
		}
	}

	for (i = 0; i < NUMENCARGADOS; i++)
	{
		if (pthread_join(hilosEncargados[i], NULL) != 0)
		{
			perror("[ERROR] Error al unir hilo de encargado.");
			return -1;
		}
	}

	for (i = 0; i < NUMTECDOMICILIARIO; i++)
	{
		if (pthread_join(hilosTecDomiciliarios[i], NULL) != 0)
		{
			perror("[ERROR] Error al unir hilo de técnico de atención domiciliaria.");
			return -1;
		}
	}

	printf("[SISTEMA] Salida controlada finalizada con éxito.\n");

	// Escribir en el log que se ha terminado la salida ordenada con éxito
	pthread_mutex_lock(&Fichero);
	writeLogMessage("SISTEMA", "Salida controlada finalizada con éxito.");
	pthread_mutex_unlock(&Fichero);

	// Liberar listas
	free(listaTecnicos);
	free(listaRespReparaciones);
	free(listaClientes);

	// Liberar hilos
	free(hilosTecnicos);
	free(hilosRespReparaciones);
	free(hilosEncargados);
	free(hilosTecDomiciliarios);

	// Eliminar los mutex al salir
	pthread_mutex_destroy(&Fichero);
	pthread_mutex_destroy(&mutexColaClientes);
	pthread_mutex_destroy(&mutexTecnicos);
	pthread_mutex_destroy(&mutexRespReparaciones);
	pthread_mutex_destroy(&mutexSolicitudesDomicilio);
	pthread_mutex_destroy(&mutexViaje);

	return 0;
}

/**FUNCIONES PRINCIPALES*/

void nuevoCliente(int tipoCliente)
{
	int i = 0;
	pthread_mutex_lock(&mutexColaClientes);
	// Busca posición libre dentro del array (id=0)
	while (i < numClientes && finalizar == 0)
	{
		if (listaClientes[i].id == 0)
		{
			// Introducimos los atributos del nuevo cliente en esa posición libre
			if (tipoCliente == 0)
			{
				contadorApp++;
				listaClientes[i].id = contadorApp;
			}
			else
			{
				contadorRed++;
				listaClientes[i].id = contadorRed;
			}

			listaClientes[i].atendido = 0;
			listaClientes[i].tipo = tipoCliente;
			listaClientes[i].solicitudDomicilio = 0;
			listaClientes[i].prioridad = calculaAleatorios(1, 10);

			// Creamos un hilo cliente y lo inicializamos
			pthread_t cliente;

			if (pthread_create(&cliente, NULL, &Cliente, &i) != 0)
			{
				perror("[ERROR] Error al introducir un nuevo cliente");
			}

			printf("[SISTEMA] Añadido cliente en posición %d\n", i + 1);

			break; // El bucle termina cuando encuentra una posición libre
		}
		i++;
	}
	pthread_mutex_unlock(&mutexColaClientes);
}

void accionesCliente(int posCliente)
{
	char *id, *msg;
	int seVa = 0; // en principio, el cliente no se va.

	id = malloc(sizeof(char) * 30);
	msg = malloc(sizeof(char) * 100);

	pthread_mutex_lock(&mutexColaClientes);
	int tipo = listaClientes[posCliente].tipo;
	pthread_mutex_unlock(&mutexColaClientes);

	if (tipo == 0)
	{
		// Cliente app
		sprintf(id, "cliapp_%d", listaClientes[posCliente].id);
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Entra al sistema un cliente de tipo APP");
		pthread_mutex_unlock(&Fichero);
	}
	else
	{
		// Cliente red
		sprintf(id, "clired_%d", listaClientes[posCliente].id);
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Entra al sistema un cliente de tipo RED");
		pthread_mutex_unlock(&Fichero);
	}

	// Calculamos si se va porque la aplicación es difícil:
	if (calculaAleatorios(0, 100) <= 10)
	{
		// Encontró la aplicación dificil
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Encuentra la aplicación difícil y se va");
		pthread_mutex_unlock(&Fichero);
		liberaCliente(posCliente);
		pthread_exit(0);
	}

	int atendido = 0; // En principio piensa que no está siendo atendido
	int tiempoEsperando = 0;
	do // Mientras que no sea atendido, comprueba cada 2 segundos
	{
		if (seVa == 1)
		{
			// Libero su espacio de la cola
			liberaCliente(posCliente);
			// Se va
			pthread_exit(0);
		}
		// ¿Estoy atendido?
		pthread_mutex_lock(&mutexColaClientes);
		atendido = listaClientes[posCliente].atendido;
		pthread_mutex_unlock(&mutexColaClientes);

		if (atendido == 0)
		{
			int porcentaje = calculaAleatorios(0, 100);

			// ¿Se cansó de esperar?
			if (tiempoEsperando % 8 == 0 && tiempoEsperando >= 8 && porcentaje <= 20)
			{
				sprintf(msg, "Se cansa de esperar tras %d segundos", tiempoEsperando);
				pthread_mutex_lock(&Fichero);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&Fichero);

				seVa = 1;
			}
			else if (porcentaje > 70 && calculaAleatorios(0, 100) <= 5)
			{
				// Perdió la conexión a internet
				pthread_mutex_lock(&Fichero);
				writeLogMessage(id, "Pierde la conexión a Internet");
				pthread_mutex_unlock(&Fichero);
				seVa = 1;
			}

			// Si no estoy atendido, espero 2 segundos
			sleep(2);
			tiempoEsperando += 2;
		}
	} while (atendido == 0);

	// Ahora ya esta siendo atendido

	while (atendido == 1)
	{
		// Estoy siendo atendido
		sleep(2);

		pthread_mutex_lock(&mutexColaClientes);
		atendido = listaClientes[posCliente].atendido;
		pthread_mutex_unlock(&mutexColaClientes);
	}

	// Ya acabó de ser atendido. Si es de tipo app, se va
	if (tipo == 1)
	{
		// Si es de tipo red
		if (calculaAleatorios(0, 10) <= 3 && finalizar == 0)
		{
			// Quiere atención domiciliaria

			// Comprobamos que el número de solicitudes no sea mayor a 4 para poder esperar a ser atendidos
			int sol;
			do
			{
				pthread_mutex_lock(&mutexSolicitudesDomicilio);
				sol = numSolicitudesDomicilio;
				pthread_mutex_unlock(&mutexSolicitudesDomicilio);

				if (sol > NUMSOLDOMINECESARIAS)
				{
					sleep(3);
				}
			} while (sol >= NUMSOLDOMINECESARIAS);

			// Ya podemos esperar a ser atendidos
			pthread_mutex_lock(&Fichero);
			writeLogMessage(id, "Espera a ser atendido en domicilio");
			pthread_mutex_unlock(&Fichero);

			// Si está el técnico de viaje, esperamos a que finalice el mismo
			pthread_mutex_lock(&mutexViaje);
			pthread_mutex_lock(&mutexColaClientes);
			listaClientes[posCliente].solicitudDomicilio = 1;
			pthread_mutex_unlock(&mutexColaClientes);
			pthread_mutex_unlock(&mutexViaje);

			pthread_mutex_lock(&mutexSolicitudesDomicilio);
			numSolicitudesDomicilio += 1;
			if (numSolicitudesDomicilio == NUMSOLDOMINECESARIAS)
			{
				// Damos el aviso al técnico de atención domiciliaria
				pthread_cond_signal(&condSolicitudesDomicilio);
			}

			pthread_mutex_lock(&mutexColaClientes);
			int estadoSolicitud = listaClientes[posCliente].solicitudDomicilio;
			pthread_mutex_unlock(&mutexColaClientes);

			while (estadoSolicitud != 0)
			{
				pthread_cond_wait(&condSolicitudesDomicilio, &mutexSolicitudesDomicilio);
				pthread_mutex_lock(&mutexColaClientes);
				estadoSolicitud = listaClientes[posCliente].solicitudDomicilio;
				pthread_mutex_unlock(&mutexColaClientes);
			}
			pthread_mutex_unlock(&mutexSolicitudesDomicilio);

			pthread_mutex_lock(&Fichero);
			writeLogMessage(id, "Recibe atención domiciliaria");
			pthread_mutex_unlock(&Fichero);
		}
	}

	// El cliente se va
	if (atendido == 2)
	{
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Se va del sistema tras ser atendido");
		pthread_mutex_unlock(&Fichero);
	}
	else
	{
		// atendido == 3
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Se va del sistema por confusión de compañía");
		pthread_mutex_unlock(&Fichero);
	}
	liberaCliente(posCliente);
	free(id);
	free(msg);
	pthread_exit(0);
}

void accionesTecnico(int tipoTrabajador, int posTrabajador)
{
	int posCliente;
	int tipoCliente;

	// Definir id del trabajador
	int numID;
	char *idTrabajador;
	idTrabajador = malloc(sizeof(char) * 30);
	if (tipoTrabajador == 0)
	{
		// Definir id de técnico
		pthread_mutex_lock(&mutexTecnicos);
		numID = listaTecnicos[posTrabajador].id;
		pthread_mutex_unlock(&mutexTecnicos);
		sprintf(idTrabajador, "tecnico_%d", numID);
	}
	else if (tipoTrabajador == 1)
	{
		// Definir id de responsable de reparaciones
		pthread_mutex_lock(&mutexRespReparaciones);
		numID = listaRespReparaciones[posTrabajador].id;
		pthread_mutex_unlock(&mutexRespReparaciones);
		sprintf(idTrabajador, "resprep_%d", numID);
	}

	// Bucle en el que el trabajador va atendiendo a los clientes que le lleguen
	do
	{
		// Comprobar si es necesario un descanso según el tipo
		int numClientesAtendidosHastaDescanso;
		if (tipoTrabajador == 0)
		{
			// Técnico

			// Obtener número de clientes atendidos hasta el descanso
			pthread_mutex_lock(&mutexTecnicos);
			numClientesAtendidosHastaDescanso = listaTecnicos[posTrabajador].numClientesAtendidosHastaDescanso;
			pthread_mutex_unlock(&mutexTecnicos);

			// El técnico descansa 5 segundos por cada 5 clientes
			if (numClientesAtendidosHastaDescanso == 5)
			{
				// Escribir en el log que va a descansar
				pthread_mutex_lock(&Fichero);
				writeLogMessage(idTrabajador, "Comienza descanso de 5 segundos");
				pthread_mutex_unlock(&Fichero);

				// El trabajador deja de estar disponible
				pthread_mutex_lock(&mutexTecnicos);
				listaTecnicos[posTrabajador].disponible = 0;
				pthread_mutex_unlock(&mutexTecnicos);

				// Descanso
				sleep(5);

				// El trabajador vuelve a estar disponible
				pthread_mutex_lock(&mutexTecnicos);
				listaTecnicos[posTrabajador].disponible = 1;
				pthread_mutex_unlock(&mutexTecnicos);

				// Escribir en el log que ha dejado de descansar
				pthread_mutex_lock(&Fichero);
				writeLogMessage(idTrabajador, "Termina su descanso de 5 segundos");
				pthread_mutex_unlock(&Fichero);

				// Resetear número de clientes atendidos a 0
				pthread_mutex_lock(&mutexTecnicos);
				listaTecnicos[posTrabajador].numClientesAtendidosHastaDescanso = 0;
				pthread_mutex_unlock(&mutexTecnicos);
			}
		}
		else if (tipoTrabajador == 1)
		{
			// Responsable de reparaciones

			// Obtener número de clientes atendidos hasta el descanso
			pthread_mutex_lock(&mutexRespReparaciones);
			numClientesAtendidosHastaDescanso = listaRespReparaciones[posTrabajador].numClientesAtendidosHastaDescanso;
			pthread_mutex_unlock(&mutexRespReparaciones);

			// El responsable de reparaciones descansa 6 segundos por cada 6 clientes
			if (numClientesAtendidosHastaDescanso == 6)
			{
				// Escribir en el log que va a descansar
				pthread_mutex_lock(&Fichero);
				writeLogMessage(idTrabajador, "Inicia descanso de 6 segundos");
				pthread_mutex_unlock(&Fichero);

				// El trabajador deja de estar disponible
				pthread_mutex_lock(&mutexRespReparaciones);
				listaRespReparaciones[posTrabajador].disponible = 0;
				pthread_mutex_unlock(&mutexRespReparaciones);

				// Descanso
				sleep(6);

				// El trabajador vuelve a estar disponible
				pthread_mutex_lock(&mutexRespReparaciones);
				listaRespReparaciones[posTrabajador].disponible = 1;
				pthread_mutex_unlock(&mutexRespReparaciones);

				// Escribir en el log que ha dejado de descansar
				pthread_mutex_lock(&Fichero);
				writeLogMessage(idTrabajador, "Termina su descanso de 6 segundos");
				pthread_mutex_unlock(&Fichero);

				// Resetear número de clientes atendidos a 0
				pthread_mutex_lock(&mutexRespReparaciones);
				listaRespReparaciones[posTrabajador].numClientesAtendidosHastaDescanso = 0;
				pthread_mutex_unlock(&mutexRespReparaciones);
			}
		}

		// Bucle hasta atender a un cliente
		do
		{
			// Obtener posición del cliente a atender según el tipo de trabajador
			pthread_mutex_lock(&mutexColaClientes);
			posCliente = obtenerPosicionProximoClienteSegunTipo(tipoTrabajador);
			// Obtener tipo del cliente en cuestión
			tipoCliente = listaClientes[posCliente].tipo;
			// El cliente pasa a estar en proceso de ser atendido
			listaClientes[posCliente].atendido = 1;
			pthread_mutex_unlock(&mutexColaClientes);
		} while (atenderCliente(tipoTrabajador, posTrabajador, tipoCliente, posCliente) != 1 && finalizar == 0);

		// Aumentar número de clientes atendidos
		if (tipoTrabajador == 0)
		{
			// Aumentar número de clientes atendidos a técnico
			pthread_mutex_lock(&mutexTecnicos);
			listaTecnicos[posTrabajador].numClientesAtendidosHastaDescanso = listaTecnicos[posTrabajador].numClientesAtendidosHastaDescanso + 1;
			pthread_mutex_unlock(&mutexTecnicos);
		}
		else if (tipoTrabajador == 1)
		{
			// Aumentar número de clientes atendidos a responsable de reparaciones
			pthread_mutex_lock(&mutexRespReparaciones);
			listaRespReparaciones[posTrabajador].numClientesAtendidosHastaDescanso = listaRespReparaciones[posTrabajador].numClientesAtendidosHastaDescanso + 1;
			pthread_mutex_unlock(&mutexRespReparaciones);
		}
	} while (finalizar == 0);
}

void accionesEncargado()
{
	int posCliente;
	int tipoCliente;

	// Bucle en el que el encargado va atendiendo a los clientes que le lleguen
	do
	{
		int numTecnicosDisponibles = obtenerNumTecnicosDisponibles();
		int numRespReparacionesDisponibles = obtenerNumRespReparacionesDisponibles();

		// Comprobar si los técnicos o responsables de reparaciones están ocupados
		if (numTecnicosDisponibles == 0 && numRespReparacionesDisponibles == 0)
		{
			// Tanto técnicos como responsables de reparaciones están ocupados

			// Obtener posición del primer cliente libre
			pthread_mutex_lock(&mutexColaClientes);
			posCliente = obtenerPosicionProximoCliente();
			// Obtener tipo del cliente en cuestión
			tipoCliente = listaClientes[posCliente].tipo;
			// El cliente pasa a estar en proceso de ser atendido
			listaClientes[posCliente].atendido = 1;
			pthread_mutex_unlock(&mutexColaClientes);
		}
		else if (numTecnicosDisponibles == 0)
		{
			// No hay técnicos disponibles, el encargado atiende al próximo cliente de su tipo

			// Obtener posición de cliente de tipo APP
			pthread_mutex_lock(&mutexColaClientes);
			posCliente = obtenerPosicionProximoClienteSegunTipo(0);
			// Obtener tipo del cliente en cuestión
			tipoCliente = listaClientes[posCliente].tipo;
			// El cliente pasa a estar en proceso de ser atendido
			listaClientes[posCliente].atendido = 1;
			pthread_mutex_unlock(&mutexColaClientes);
		}
		else if (numRespReparacionesDisponibles == 0)
		{
			// No hay responsables de reparaciones disponibles, el encargado atiende al próximo cliente de su tipo

			// Obtener posición de cliente de tipo RED
			pthread_mutex_lock(&mutexColaClientes);
			posCliente = obtenerPosicionProximoClienteSegunTipo(1);
			// Obtener tipo del cliente en cuestión
			tipoCliente = listaClientes[posCliente].tipo;
			// El cliente pasa a estar en proceso de ser atendido
			listaClientes[posCliente].atendido = 1;
			pthread_mutex_unlock(&mutexColaClientes);
		}
		else
		{
			// No hay trabajadores ocupados
			continue;
		}

		// Atender al cliente como encargado si hay trabajadores ocupados
		atenderCliente(-1, 1, tipoCliente, posCliente);
	} while (finalizar == 0);
}

void accionesTecnicoDomiciliario()
{
	char *id, *cadena1, *cadena2;
	int i;
	int posicionCliente;
	int totalSolicitudes = NUMSOLDOMINECESARIAS;

	id = malloc(sizeof(char) * 30);
	cadena1 = malloc(sizeof(char) * 50);
	cadena2 = malloc(sizeof(char) * 50);

	sprintf(id, "tecnico_dom");
	do
	{
		// Comprobamos que el número de solicitudes domiciliarias. Bloqueamos si es menor que 4
		pthread_mutex_lock(&mutexSolicitudesDomicilio);
		while (numSolicitudesDomicilio < NUMSOLDOMINECESARIAS)
		{
			// Espera a que el número de solicitudes sea 4 solo si no se debe terminar ya el programa
			if (finalizar == 0)
			{
				pthread_cond_wait(&condSolicitudesDomicilio, &mutexSolicitudesDomicilio);
			}
			// Atiende las solicitudes pendientes cuando finaliza el programa.
			else if (finalizar == 1)
			{
				totalSolicitudes = numSolicitudesDomicilio;
				break;
			}
		}
		pthread_mutex_unlock(&mutexSolicitudesDomicilio);

		// Si no hay solicitudes, salimos del bucle (salida controlada)
		if (numSolicitudesDomicilio == 0)
		{
			break;
		}

		// El técnico pasa a estar de viaje, no se atenderán más solicitudes hasta que vuelve
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Empieza la atención domiciliaria");
		pthread_mutex_unlock(&Fichero);

		pthread_mutex_lock(&mutexViaje);
		// Atendemos cada solicitud
		for (i = 0; i < totalSolicitudes; i++)
		{
			sleep(1);
			sprintf(cadena1, "Termina de atender al cliente ");

			// Por si justo se cambiara el número máximo de clientes en la cola, dentro de mutex
			// pthread_mutex_lock(&mutexColaClientes);
			atenderClienteAttDom(cadena2);
			// pthread_mutex_unlock(&mutexColaClientes);

			strcat(cadena1, cadena2);

			pthread_mutex_lock(&Fichero);
			writeLogMessage(id, cadena1);
			pthread_mutex_unlock(&Fichero);
		}

		// Reseteamos el número de solicitudes a domicilio. Se finaliza la atención domiciliaria.
		pthread_mutex_lock(&mutexSolicitudesDomicilio);
		numSolicitudesDomicilio = 0;
		pthread_mutex_unlock(&mutexSolicitudesDomicilio);

		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Termina la atención domiciliaria");
		pthread_mutex_unlock(&Fichero);
		pthread_mutex_unlock(&mutexViaje);

		// Damos aviso a los que esperaban por atención domiciliaria
		pthread_mutex_lock(&mutexSolicitudesDomicilio);
		for (i = 0; i < totalSolicitudes; i++)
		{
			pthread_cond_signal(&condSolicitudesDomicilio);
		}
		pthread_mutex_unlock(&mutexSolicitudesDomicilio);

	} while (finalizar == 0);

	free(id);
	free(cadena1);
	free(cadena2);
}

/**FUNCIONES AUXILIARES*/

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

int calculaAleatorios(int min, int max)
{
	return rand() % (max - min + 1) + min;
}

void liberaCliente(int posicion)
{
	pthread_mutex_lock(&mutexColaClientes);
	listaClientes[posicion].id = 0;
	listaClientes[posicion].atendido = 0;
	listaClientes[posicion].prioridad = 0;
	listaClientes[posicion].solicitudDomicilio = 0;
	listaClientes[posicion].tipo = 0;
	pthread_mutex_unlock(&mutexColaClientes);
}

int obtenerPosicionProximoCliente()
{
	// Variables que guardarán los datos del próximo cliente
	int posProxCliente = -1;
	int prioridadProxCliente = -1;
	int tipoProxCliente = 0;
	int idProxCliente = -1;

	int cambiarCliente = 0;

	for (int i = 0; i < numClientes; i++)
	{
		cambiarCliente = 0;
		if (listaClientes[i].id != 0 && listaClientes[i].atendido == 0)
		{
			// Es un cliente, no está vacío y puede ser atendido.
			if (listaClientes[i].tipo == 1 && tipoProxCliente == 0)
			{
				// Tenemos un cliente de tipo preferido frente a uno no preferido. Lo cojemos sí o sí
				// Se debe actualizar los campos por el nuevo cliente
				cambiarCliente = 1;
			}
			else if (listaClientes[i].tipo == tipoProxCliente && tipoProxCliente == 1 || listaClientes[i].tipo == tipoProxCliente && tipoProxCliente == 0)
			{
				// Tenemos dos clientes del mismo tipo
				if (listaClientes[i].prioridad > prioridadProxCliente)
				{
					// Tiene más prioridad, luego cambiamos el cliente
					// Se debe actualizar los campos por el nuevo cliente
					cambiarCliente = 1;
				}
				else if (listaClientes[i].prioridad == prioridadProxCliente)
				{
					// Tienen la misma prioridad, cogemos el de menor id (más tiempo esperado)
					if (listaClientes[i].id < idProxCliente)
					{
						// Se debe actualizar los campos por el nuevo cliente
						cambiarCliente = 1;
					}
				}
			}
		}

		// Comprobar si se deben modificar los campos por este cliente
		if (cambiarCliente == 1)
		{
			posProxCliente = i;
			prioridadProxCliente = listaClientes[i].prioridad;
			tipoProxCliente = listaClientes[i].tipo;
			idProxCliente = listaClientes[i].id;
		}
	}

	return posProxCliente;
}

int obtenerPosicionProximoClienteSegunTipo(int tipoCliente)
{
	// Variables que guardarán los datos del próximo cliente
	int posProxCliente = -1;
	int prioridadProxCliente = -1;
	int idProxCliente = -1;

	int cambiarCliente = 0;

	for (int i = 0; i < numClientes; i++)
	{
		cambiarCliente = 0;
		if (listaClientes[i].id != 0 && listaClientes[i].atendido == 0)
		{
			// Es un cliente, no está vacío y puede ser atendido.
			if (listaClientes[i].tipo == tipoCliente)
			{
				// Tenemos dos clientes del mismo tipo
				if (listaClientes[i].prioridad > prioridadProxCliente)
				{
					// Tiene más prioridad, luego cambiamos el cliente
					// Se debe actualizar los campos por el nuevo cliente
					cambiarCliente = 1;
				}
				else if (listaClientes[i].prioridad == prioridadProxCliente)
				{
					// Tienen la misma prioridad, cogemos el de menor id (más tiempo esperado)
					if (listaClientes[i].id < idProxCliente)
					{
						// Se debe actualizar los campos por el nuevo cliente
						cambiarCliente = 1;
					}
				}
			}
		}

		// Comprobar si se deben modificar los campos por este cliente
		if (cambiarCliente == 1)
		{
			posProxCliente = i;
			prioridadProxCliente = listaClientes[i].prioridad;
			idProxCliente = listaClientes[i].id;
		}
	}

	return posProxCliente;
}

int atenderCliente(int tipoTrabajador, int posTrabajador, int tipoCliente, int posCliente)
{

	// Establecer como no disponible al trabajador según tipo
	if (tipoTrabajador == 0)
	{
		pthread_mutex_lock(&mutexTecnicos);
		listaTecnicos[posTrabajador].disponible = 0;
		pthread_mutex_unlock(&mutexTecnicos);
	}
	else if (tipoTrabajador == 1)
	{
		pthread_mutex_lock(&mutexTecnicos);
		listaRespReparaciones[posTrabajador].disponible = 0;
		pthread_mutex_unlock(&mutexTecnicos);
	}

	if (posCliente == -1)
	{
		// No se ha encontrado un cliente al que atender, esperamos 2 segundos
		sleep(2);
		// No se ha atendido un cliente
		return 0;
	}
	else
	{
		char *idTrabajador, *idCliente, *msg;
		idTrabajador = malloc(sizeof(char) * 30);
		idCliente = malloc(sizeof(char) * 30);
		msg = malloc(sizeof(char) * 100);

		// Definir id del trabajador según su tipo
		int numID = 1;
		if (tipoTrabajador == -1)
		{
			// Definir id del encargado
			sprintf(idTrabajador, "encargado_%d", numID);
		}
		else if (tipoTrabajador == 0)
		{
			// Definir id de técnico
			pthread_mutex_lock(&mutexTecnicos);
			numID = listaTecnicos[posTrabajador].id;
			pthread_mutex_unlock(&mutexTecnicos);
			sprintf(idTrabajador, "tecnico_%d", numID);
		}
		else if (tipoTrabajador == 1)
		{
			// Definir id de responsable de reparaciones
			pthread_mutex_lock(&mutexRespReparaciones);
			numID = listaRespReparaciones[posTrabajador].id;
			pthread_mutex_unlock(&mutexRespReparaciones);
			sprintf(idTrabajador, "resprep_%d", numID);
		}

		// Definir id del cliente según su tipo
		if (tipoCliente == 0)
		{
			// Definir id de cliente de red
			numID = listaClientes[posCliente].id;
			sprintf(idCliente, "cliapp_%d", numID);
		}
		else
		{
			// Definir id de cliente de app
			numID = listaClientes[posCliente].id;
			sprintf(idCliente, "clired_%d", numID);
		}

		// Indicamos que comienza el proceso de atención en el log
		pthread_mutex_lock(&Fichero);
		sprintf(msg, "Comienza a atender a %s", idCliente);
		writeLogMessage(idTrabajador, msg);
		pthread_mutex_unlock(&Fichero);

		// Calcular tipo de atención
		int tipoAtencion = calculaAleatorios(1, 10);

		if (tipoAtencion == 1)
		{
			// Cliente mal identificado

			// Atendemos al cliente...
			sleep(calculaAleatorios(2, 6));

			// Indicamos que finaliza la atención y el motivo en el log
			pthread_mutex_lock(&Fichero);
			sprintf(msg, "Termina de atender a %s. Está mal identificado", idCliente);
			writeLogMessage(idTrabajador, msg);
			pthread_mutex_unlock(&Fichero);

			// Marcar cliente como atendido
			pthread_mutex_lock(&mutexColaClientes);
			listaClientes[posCliente].atendido = 2;
			pthread_mutex_unlock(&mutexColaClientes);
		}
		else if (tipoAtencion == 2)
		{
			// Cliente confundido

			// Atendemos al cliente...
			sleep(calculaAleatorios(1, 2));

			// Indicamos que finaliza la atención y el motivo en el log
			pthread_mutex_lock(&Fichero);
			sprintf(msg, "Termina de atender a %s. Está confundido de compañía", idCliente);
			writeLogMessage(idTrabajador, msg);
			pthread_mutex_unlock(&Fichero);

			// Marcar cliente como confundido
			pthread_mutex_lock(&mutexColaClientes);
			listaClientes[posCliente].atendido = 3;
			pthread_mutex_unlock(&mutexColaClientes);
		}
		else
		{
			// Cliente con todo en regla

			// Atendemos al cliente...
			sleep(calculaAleatorios(1, 4));

			// Indicamos que finaliza la atención y el motivo en el log
			pthread_mutex_lock(&Fichero);
			sprintf(msg, "Termina de atender a %s. Tiene todo en regla", idCliente);
			writeLogMessage(idTrabajador, msg);
			pthread_mutex_unlock(&Fichero);

			pthread_mutex_lock(&mutexColaClientes);
			listaClientes[posCliente].atendido = 2;
			pthread_mutex_unlock(&mutexColaClientes);
		}
		free(idTrabajador);
		free(idCliente);
		free(msg);
	}

	// Establecer como disponible al trabajador segun tipo
	if (tipoTrabajador == 0)
	{
		pthread_mutex_lock(&mutexTecnicos);
		listaTecnicos[posTrabajador].disponible = 1;
		pthread_mutex_unlock(&mutexTecnicos);
	}
	else if (tipoTrabajador == 1)
	{
		pthread_mutex_lock(&mutexTecnicos);
		listaRespReparaciones[posTrabajador].disponible = 1;
		pthread_mutex_unlock(&mutexTecnicos);
	}

	// Se ha atendido a un cliente
	return 1;
}

void atenderClienteAttDom(char *cadena)
{
	int i, count;
	count = 0;
	i = 0;
	while (i < numClientes)
	{
		if (listaClientes[i].solicitudDomicilio == 1)
		{

			sprintf(cadena, "clired_%d", listaClientes[i].id);
			pthread_mutex_lock(&mutexColaClientes);
			listaClientes[i].solicitudDomicilio = 0;
			pthread_mutex_unlock(&mutexColaClientes);
			break;
		}
		else
		{
			i++;
		}
	}
}

int obtenerNumTecnicosDisponibles()
{
	int tdisponibles = 0;

	pthread_mutex_lock(&mutexTecnicos);

	for (int i = 0; i < numTecnicos; i++)
	{
		if (listaTecnicos[i].disponible == 1)
		{
			tdisponibles++;
		}
	}

	pthread_mutex_unlock(&mutexTecnicos);

	return tdisponibles;
}

int obtenerNumRespReparacionesDisponibles()
{
	int rdisponibles = 0;

	pthread_mutex_lock(&mutexRespReparaciones);

	for (int i = 0; i < NUMRESPREPARACIONES; i++)
	{
		if (listaRespReparaciones[i].disponible == 1)
		{
			rdisponibles++;
		}
	}

	pthread_mutex_unlock(&mutexRespReparaciones);

	return rdisponibles;
}

void activarSenales()
{
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

	sig.sa_handler = handlerAmpliaClientes;
	if (sigaction(SIGPIPE, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}

	sig.sa_handler = handlerAmpliaTecnicos;
	if (sigaction(SIGALRM, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}
}

void desactivarSenales()
{
	sig.sa_handler = SIG_IGN;
	if (sigaction(SIGUSR1, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}

	sig.sa_handler = SIG_IGN;
	if (sigaction(SIGUSR2, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}

	sig.sa_handler = SIG_IGN;
	if (sigaction(SIGINT, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}

	sig.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}

	sig.sa_handler = SIG_IGN;
	if (sigaction(SIGALRM, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}
}

void printWelcome()
{
	printf("=====================================================================\n");
	printf("|      BIENVENIDO AL SISTEMA DE GESTIÓN DE AVERÍAS luZECita         |\n");
	printf("|===================================================================|\n");
	printf("|     Grupo 1:                                                      |\n");
	printf("|       Mario López Barazón (mlopeb04)                              |\n");
	printf("|       Guillermo Martínez Martínez (gmartm08)                      |\n");
	printf("|       Daniel Martínez Sánchez (dmarts05)                          |\n");
	printf("|       Álvaro Prieto Álvarez (apriea04)                            |\n");
	printf("|===================================================================|\n");
	printf("| Para utilizar el programa, siga las instrucciones de README.md    |\n");
	printf("| Consulte el fichero registroTiempos.log para ver más detalles     |\n");
	printf("=====================================================================\n");
}
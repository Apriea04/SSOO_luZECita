#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#define NCLIENTES 20
#define NTECNICOS 2
#define NRESPREPARACIONES 2
#define NENCARGADOS 1
#define NTECDOMICILIARIA 1
#define NSOLICDOMINECESARIAS 4 // Numero de solicitudes domciliarias necesarias para comenzar con esa atención


/**DECLARACIONES GLOBALES*/

// Mutex
pthread_mutex_t Fichero, colaClientes, mutexTecnicos, mutexResponsables, solicitudes;

// Variables condición
pthread_cond_t condSolicitudesDomicilio;

// Contadores
int contadorApp, contadorRed, numSolicitudesDomicilio;

// Variable para llevar a cabo la salida controlada
int finalizar;

// Sigaction
struct sigaction sig;

struct Cliente
{
	int id;		   // Número secuencial comenzando en 1 para su tipo de cliente
	int atendido;  // 0 SI NO ATENDIDO; 1 SI ESTÁ EN PROCESO; 2 SI ESTÁ ATENDIDO; 3 SI SE CONFUNDE O MAL IDENTIFICADO
	int tipo;	   // 0 SI APP; 1 SI RED
	int prioridad; // Del 1 al 10 aleatoria. 10 es la prioridad más alta
	int solicitud; // 0 si no solicita atención domiciliaria Ó ya se le atendió domiciliariamente; 1 si sí la solicita
};

struct Trabajador
{
	int id;					  // Número secuencial comenzando en 1 para cada trabajador
	int disponible;			  // 0 si no está disponible, 1 si está disponible
	int numClientesAtendidos; // Número clientes atendidos por el técnico
};

// Listas de hilos trabajadores
pthread_t tecnicos[NTECNICOS];
pthread_t respReparaciones[NRESPREPARACIONES];
pthread_t encargados[NENCARGADOS];
pthread_t tecAttDomiciliaria[NTECDOMICILIARIA];

// Lista de clientes
struct Cliente listaClientes[NCLIENTES];

// Lista de técnicos
struct Trabajador *listaTecnicos;

// Lista de responsables de reparaciones
struct Trabajador *listaRespReparaciones;

FILE *logFile;

/**DECLARACIÓN DE FUNCIONES PRINCIPALES*/

// GUILLERMO
/**
 * Introduce un cliente en la cola de clientes cuando se recibe una señal.
 * Cuando aparece un cliente, lo introduce en la primera posición libre que encuentre (Cuando  id==0).
 * Si no hay posiciones libres, no se hace nada y se ignora el cliente.
 *
 * @param tipo
 */
void nuevoCliente(int tipo);

// ÁLVARO
/**
 * Recibe un puntero a su estructura cliente de la cola de clientes.
 * posicion (int): valor entero que indica la posición de un cliente en el array listaClientes (struct[]).
 * @param posicion
 */
void accionesCliente(int posicion);

// DANIEL
/**
 * LLeva a cabo las funciones de un técnico o
 * responsable de reparaciones.
 *
 * tipoTrabajador (int): entero que representa si un trabajador
 * 	                  se encarga de la app o de la red.
 * posTrabajador (int): entero con la posición en la lista del trabajador.
 * @param tipoTrabajador
 * @param posTrabajador
 */
void accionesTecnico(int tipoTrabajador, int posTrabajador);

// MARIO
/**
 * LLeva a cabo las funciones de un encargado.
 */
void accionesEncargado();

void accionesTecnicoDomiciliario();

/**DECLARACIÓN DE FUNCIONES AUXILIARES*/

/**
 * Escribe un mensaje nuevo en el fichero registroTiempos.log.
 * El mensaje estará compuesto por la fecha y la hora, el identificador del hilo que lo ejecuta y el mensaje.
 * id (*char): puntero que contiene la cadena de caracteres con el identificador de un hilo.
 * msg (*char): puntero que contiene la cadena de caracteres con el mensaje a escribir en el log.
 * @param id
 * @param msg
 */
void writeLogMessage(char *id, char *msg);

/**
 * Calcula un número aleatorio entre el mínimo y el máximo espeficicados.
 * Ambos números se incluyen en el cálculo.
 * min (int): mínimo valor del intervalo.
 * max (int): máximo valor del intervalo.
 * @param min
 * @param max
 */
int calculaAleatorios(int min, int max);

// TODO: ESTA FUNCIÓN NO SE ESTÁ UTILIZANDO
/**
 * Devuelve el número de clientes de tipo red.
 */
int NClientesRed();

/**
 * Libera el cliente del vector de clientes.
 * posicion (int): valor entero que indica la posición de un cliente en el array listaClientes (struct[]).
 * @param posicion
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
 * tipoCliente (int): tipo de cliente que se va a buscar.
 * @param tipoCliente
 */
int obtenerPosicionProximoClienteSegunTipo(int tipoCliente);

/**
 * Lleva a cabo el proceso de atención de un técnico a un cliente
 *
 * tipoTrabajador (int): tipo de técnico que está atendiendo al cliente
 * posTrabajador (int): posición del técnico en su respectiva lista
 * tipoCliente (int): tipo de cliente
 * posCliente (int): posición del cliente en la lista de clientes
 */
void atenderCliente(int tipoTrabajador, int posTrabajador, int tipoCliente, int posCliente);

/**
 * Escribe en el puntero el identificador del próximo cliente que recibió atención domiciliaria y
 * establece la solicitud a 0 del cliente que la solicitó.
 */
int obtenerIDClienteAttDom(char *cadena);

/**
 * Devuelve un cero si no hay técnicos disponibles, en caso contrario, devuelve el número de
 * técnicos disponibles.
 */
int obtenerNumTecnicosDisponibles();

/**
 * Devuelve un cero si no hay responsables disponibles, en caso contrario, devuelve el número de
 * responsables disponibles.
 */
int obtenerNumRespReparacionesDisponibles();

/**
 * Imprime el mensaje de bienvenida.
 */
void printWelcome();

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

void handlerEmpty(int sig){}

/**
 * Manejadora de señal que finaliza de manera ordenada el programa. Responde a la señal SIGINT.
 * @param s 
 */
void handlerTerminar(int s)
{
	finalizar = 1;
	sig.sa_handler = handlerEmpty;
	if (sigaction(SIGUSR1, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}

	sig.sa_handler = handlerEmpty;
	if (sigaction(SIGUSR2, &sig, NULL) == -1)
	{
		perror("[ERROR] Error en la llamada a sigaction.");
		exit(-1);
	}
	printf("Hola SIGINT\n");
	//printf("Total de clientes: %d\n", contadorApp + contadorRed);
}

/**CÓDIGOS DE EJECUCIÓN DE HILOS*/

/**
 * Código ejecutado por los hilos de técnicos. 
 * @param arg 
 * @return void* 
 */
void *Tecnico(void *arg)
{
	int index = *(int *)arg;
	accionesTecnico(0, index);
	free(arg);
}

/**
 * Código ejecutado por los hilos de responsables de reparaciones.
 * @param arg 
 * @return void* 
 */
void *Responsable(void *arg)
{
	int index = *(int *)arg;
	accionesTecnico(1, index);
	free(arg);
}

/**
 * Código ejecutado por el hilo de encargado.
 * @param arg 
 * @return void* 
 */
void *Encargado(void *arg)
{
	int index = *(int *)arg;
	accionesEncargado();
	printf("Encargado %d\n", index);
	free(arg);
}

/**
 * Código ejecutado por el técnico de atención domiciliaria.
 * @param arg 
 * @return void* 
 */
void *AtencionDomiciliaria(void *arg)
{
	int index = *(int *)arg;
	accionesTecnicoDomiciliario();
	printf("Atención domiciliaria %d\n", index);
	free(arg);
}

/**
 * Código ejecutado por los clientes entrantes al sistema. 
 * @param arg 
 * @return void* 
 */
void *Cliente(void *arg)
{
	int index = *(int *)arg;
	accionesCliente(index);
	free(arg);
}


/**MAIN*/

int main()
{
	printWelcome();

	// Definir manejadoras para señales
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
	pthread_mutex_init(&mutexTecnicos, NULL);
	pthread_mutex_init(&mutexResponsables, NULL);
	pthread_mutex_init(&solicitudes, NULL);

	// Inicialización de variables condición
	pthread_cond_init(&condSolicitudesDomicilio, NULL);

	// Limpiar log
	pthread_mutex_lock(&Fichero);
	logFile = fopen("registroTiempos.log", "w");
	fclose(logFile);
	pthread_mutex_unlock(&Fichero);

	// Inicialización de contadores
	contadorApp = 0;
	contadorRed = 0;
	numSolicitudesDomicilio = 0;
	finalizar = 0;

	// Inicialización de lista de clientes
	for (i = 0; i < NCLIENTES; i++)
	{
		listaClientes[i].id = 0;
		listaClientes[i].prioridad = 0;
		listaClientes[i].atendido = 0;
		listaClientes[i].solicitud = 0;
	}

	// Inicialización de lista de técnicos
	listaTecnicos = (struct Trabajador *)malloc(NTECNICOS * sizeof(struct Trabajador));
	for (i = 0; i < NTECNICOS; i++)
	{
		listaTecnicos[i].id = 0;
		// Por defecto disponible
		listaTecnicos[i].disponible = 1;
		listaTecnicos[i].numClientesAtendidos = 0;
	}

	// Inicialización de técnicos
	for (i = 0; i < NTECNICOS; i++)
	{
		// Agregar a lista de técnicos
		listaTecnicos[i].id = i + 1;

		int *index = malloc(sizeof(int));
		*index = i;
		if (pthread_create(&tecnicos[i], NULL, &Tecnico, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de técnico.");
			return -1;
		}
	}

	// Inicialización de lista de reponsables de reparaciones.
	listaRespReparaciones = (struct Trabajador *)malloc(NRESPREPARACIONES * sizeof(struct Trabajador));
	for (i = 0; i < NRESPREPARACIONES; i++)
	{
		listaRespReparaciones[i].id = 0;
		listaRespReparaciones[i].disponible = 1;
		listaRespReparaciones[i].numClientesAtendidos = 0;
	}

	// Inicialización de responsables de reparaciones.
	for (i = 0; i < NRESPREPARACIONES; i++)
	{
		// Agregar a lista de responsables de reparaciones.
		listaRespReparaciones[i].id = i + 1;

		int *index = malloc(sizeof(int));
		*index = i;
		if (pthread_create(&respReparaciones[i], NULL, &Responsable, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de responsable de reparaciones.");
			return -1;
		}
	}

	// Inicialización de encargados.
	for (i = 0; i < NENCARGADOS; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&encargados[i], NULL, &Encargado, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de encargado.");
			return -1;
		}
	}

	// Inicialización de técnicos de atención domiciliaria.
	for (i = 0; i < NTECDOMICILIARIA; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&tecAttDomiciliaria[i], NULL, &AtencionDomiciliaria, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de técnico de atención domiciliaria.");
			return -1;
		}
	}

	while (finalizar == 0)
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
		pause();
	}

	// TODO: No sé si esto es necesario, conviene revisarlo posteriormente - Joins de todos los hilos
	// for (i = 0; i < NTECNICOS; i++)
	// {
	// 	if (pthread_join(tecnicos[i], NULL) != 0)
	// 	{
	// 		perror("[ERROR] Error al unir hilo de técnico.");
	// 		return -1;
	// 	}
	// }

	// for (i = 0; i < NRESPREPARACIONES; i++)
	// {
	// 	if (pthread_join(respReparaciones[i], NULL) != 0)
	// 	{
	// 		perror("[ERROR] Error al unir hilo de responsable de reparaciones.");
	// 		return -1;
	// 	}
	// }

	// for (i = 0; i < NENCARGADOS; i++)
	// {
	// 	if (pthread_join(encargados[i], NULL) != 0)
	// 	{
	// 		perror("[ERROR] Error al unir hilo de encargado.");
	// 		return -1;
	// 	}
	// }

	// for (i = 0; i < NTECDOMICILIARIA; i++)
	// {
	// 	if (pthread_join(tecAttDomiciliaria[i], NULL) != 0)
	// 	{
	// 		perror("[ERROR] Error al unir hilo de técnico de atención domiciliaria.");
	// 		return -1;
	// 	}
	// }

	// Liberar listas
	free(listaTecnicos);
	free(listaRespReparaciones);

	// Eliminar los mutex al salir
	pthread_mutex_destroy(&Fichero);
	pthread_mutex_destroy(&colaClientes);
	pthread_mutex_destroy(&mutexTecnicos);
	pthread_mutex_destroy(&mutexResponsables);
	pthread_mutex_destroy(&solicitudes);

	return 0;
}

/**FUNCIONES PRINCIPALES*/

void nuevoCliente(int tipo)
{
	int i = 0;
	pthread_mutex_lock(&colaClientes);
	// Busca posición libre dentro del array (id=0)
	while (i < NCLIENTES && finalizar == 0)
	{
		if (listaClientes[i].id == 0)
		{
			// Introducimos los atributos del nuevo cliente en esa posición libre
			if (tipo == 0)
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
			listaClientes[i].tipo = tipo;
			listaClientes[i].solicitud = 0;
			listaClientes[i].prioridad = calculaAleatorios(1, 10);
			struct Cliente *punteroCliente = malloc(sizeof(struct Cliente));
			*(punteroCliente) = listaClientes[i];

			// Creamos un hilo cliente y lo inicializamos
			pthread_t cliente;
			char *id;

			id = malloc(sizeof(char) * 20); // Identificador único de cada cliente

			// Inicializamos hilo cliente de tipo APP y escribimos el hecho en el log
			if (listaClientes[i].tipo == 0)
			{
				sprintf(id, "cliapp_%d", listaClientes[i].id);
				pthread_mutex_lock(&Fichero);
				writeLogMessage(id, "Nuevo cliente de tipo APP ha entrado en la cola.");
				pthread_mutex_unlock(&Fichero);
			}
			// Inicializamos hilo cliente de tipo RED y escribimos el hecho en el log
			else
			{
				sprintf(id, "clired_%d", listaClientes[i].id);
				pthread_mutex_lock(&Fichero);
				writeLogMessage(id, "Nuevo cliente de tipo RED ha entrado en cola.");
				pthread_mutex_unlock(&Fichero);
			}

			if (pthread_create(&cliente, NULL, &Cliente, &i) != 0)
			{
				perror("[ERROR] Error al introducir un nuevo cliente");
			}

			break; // El bucle termina cuando encuentra una posición libre
		}
		i++;
	}
	pthread_mutex_unlock(&colaClientes);
}

void accionesCliente(int posicion)
{
	struct Cliente *cliente = malloc(sizeof(struct Cliente));
	printf("Nuevo cliente en el sistema. Posición: %d\n", posicion);
	*cliente = listaClientes[posicion];

	char *id, *msg;
	int seVa = 0; // en principio, el cliente no se va.

	id = malloc(sizeof(char) * 20);

	pthread_mutex_lock(&colaClientes); // TODO duda ¿Proteger?
	int tipo = listaClientes[posicion].tipo;
	pthread_mutex_unlock(&colaClientes);

	if (tipo == 0)
	{
		// Cliente app
		sprintf(id, "cliapp_%d", cliente->id);
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Cliente de tipo APP acaba de entrar al sistema.");
		pthread_mutex_unlock(&Fichero);
	}
	else
	{
		// Cliente red
		sprintf(id, "clired_%d", cliente->id);
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Cliente de tipo RED acaba de entrar al sistema.");
		pthread_mutex_unlock(&Fichero);
	}

	// Calculamos si se va porque la aplicación es difícil:
	if (calculaAleatorios(0, 100) <= 10)
	{
		// Encontró la aplicación dificil
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Encontró la aplicación difícil y se fue.");
		pthread_mutex_unlock(&Fichero);
		liberaCliente(posicion);
		pthread_exit(0);
	}

	int atendido = 0; // En principio piensa que no está siendo atendido
	int tiempoEsperando = 0;
	do // Mientras que no sea atendido, comprueba cada 2 segundos
	{
		if (seVa == 1)
		{
			// Libero su espacio de la cola
			liberaCliente(posicion);
			// Se va
			pthread_exit(0);
		}
		// ¿Estoy atendido?
		pthread_mutex_lock(&colaClientes);
		atendido = listaClientes[posicion].atendido;
		pthread_mutex_unlock(&colaClientes);

		if (atendido == 0)
		{
			int porcentaje = calculaAleatorios(0, 100);

			// ¿Se cansó de esperar?
			if (tiempoEsperando >= 8 && porcentaje <= 20)
			{
				sprintf(msg, "Se cansó de esperar tras %d segundos", tiempoEsperando);
				pthread_mutex_lock(&Fichero);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&Fichero);

				seVa = 1;
			}
			else if (porcentaje > 70 && calculaAleatorios(0, 100) <= 5)
			{
				// Perdió la conexión a internet
				pthread_mutex_lock(&Fichero);
				writeLogMessage(id, "Perdió la conexión a Internet.");
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

		pthread_mutex_lock(&colaClientes);
		atendido = listaClientes[posicion].atendido;
		pthread_mutex_unlock(&colaClientes);
	}

	// Ya acabó de ser atendido. Si es de tipo app, se va
	if (tipo == 1)
	{
		// Si es de tipo red
		if (calculaAleatorios(0, 10) <= 3)
		{
			// Y quiere atención domiciliaria
			// TODO: comprobar este bloque de código
			int sol;
			do
			{
				pthread_mutex_lock(&solicitudes);
				sol = numSolicitudesDomicilio;
				pthread_mutex_unlock(&solicitudes);

				if (sol > NSOLICDOMINECESARIAS)
				{
					sleep(3);
				}
			} while (sol > NSOLICDOMINECESARIAS);

			// numSolicitudesDomicilio <= 4

			pthread_mutex_lock(&Fichero);
			writeLogMessage(id, "Esperando a ser atendido en domicilio.");
			pthread_mutex_unlock(&Fichero);

			pthread_mutex_lock(&colaClientes);
			listaClientes[posicion].solicitud = 1;
			pthread_mutex_unlock(&colaClientes);

			pthread_mutex_lock(&solicitudes);
			numSolicitudesDomicilio += 1;
			if (numSolicitudesDomicilio == NSOLICDOMINECESARIAS)
			{
				// Damos el aviso:
				pthread_cond_signal(&condSolicitudesDomicilio);
			}

			pthread_mutex_lock(&colaClientes);
			int estadoSolicitud = listaClientes[posicion].solicitud;
			pthread_mutex_unlock(&colaClientes);

			while (estadoSolicitud != 0)
			{
				pthread_cond_wait(&condSolicitudesDomicilio, &solicitudes);
				pthread_mutex_lock(&colaClientes);
				estadoSolicitud = listaClientes[posicion].solicitud;
				pthread_mutex_unlock(&colaClientes);
			}
			pthread_mutex_unlock(&solicitudes);

			

			pthread_mutex_lock(&Fichero);
			writeLogMessage(id, "Recibió atención domiciliaria.");
			pthread_mutex_unlock(&Fichero);
		}
	}

	// El cliente se va
	liberaCliente(posicion);
	pthread_mutex_lock(&Fichero);
	writeLogMessage(id, "Se va tras haber sido atendido");
	pthread_mutex_unlock(&Fichero);
	pthread_exit(0);
}

void accionesTecnico(int tipoTrabajador, int posTrabajador)
{
	int posCliente;
	int tipoCliente;

	//TODO cambiar estado de disponibilidad del trabajador

	// Bucle en el que el trabajador va atendiendo a los clientes que le lleguen
	do
	{
		// Comprobar si es necesario un descanso según el tipo
		int numClientesAtendidos;
		if (tipoTrabajador == 0)
		{
			// Técnico
			pthread_mutex_lock(&mutexTecnicos);
			numClientesAtendidos = listaTecnicos[posTrabajador].numClientesAtendidos;
			pthread_mutex_unlock(&mutexTecnicos);

			// El técnico descansa 5 segundos por cada 5 clientes
			if (numClientesAtendidos % 5 == 0)
			{
				sleep(5);
			}
		}
		else if (tipoTrabajador == 1)
		{
			// Responsable de reparaciones
			pthread_mutex_lock(&mutexResponsables);
			numClientesAtendidos = listaRespReparaciones[posTrabajador].numClientesAtendidos;
			pthread_mutex_unlock(&mutexResponsables);

			// El responsable de reparaciones descansa 6 segundos por cada 6 clientes
			if (numClientesAtendidos % 6 == 0)
			{
				sleep(6);
			}
		}

		// Obtener posición del cliente a atender según el tipo de trabajador
		pthread_mutex_lock(&colaClientes);
		posCliente = obtenerPosicionProximoClienteSegunTipo(tipoTrabajador);
		// Obtener tipo del cliente en cuestión
		tipoCliente = listaClientes[posCliente].tipo;
		// El cliente pasa a estar en proceso de ser atendido
		listaClientes[posCliente].atendido = 1;
		pthread_mutex_unlock(&colaClientes);

		// Atender al cliente
		atenderCliente(tipoTrabajador, posTrabajador, tipoCliente, posCliente);

		// Aumentar número de clientes atendidos
		if (tipoTrabajador == 0)
		{
			// Aumentar número de clientes atendidos a técnico
			pthread_mutex_lock(&mutexTecnicos);
			listaTecnicos[posTrabajador].numClientesAtendidos = listaTecnicos[posTrabajador].numClientesAtendidos + 1;
			pthread_mutex_unlock(&mutexTecnicos);
		}
		else if (tipoTrabajador == 1)
		{
			// Aumentar número de clientes atendidos a responsable de reparaciones
			pthread_mutex_lock(&mutexResponsables);
			listaRespReparaciones[posTrabajador].numClientesAtendidos = listaRespReparaciones[posTrabajador].numClientesAtendidos + 1;
			pthread_mutex_unlock(&mutexResponsables);
		}
	} while (1);
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
			pthread_mutex_lock(&colaClientes);
			posCliente = obtenerPosicionProximoCliente();
			// Obtener tipo del cliente en cuestión
			tipoCliente = listaClientes[posCliente].tipo;
			// El cliente pasa a estar en proceso de ser atendido
			listaClientes[posCliente].atendido = 1;
			pthread_mutex_unlock(&colaClientes);
		}
		else if (numTecnicosDisponibles == 0)
		{
			// No hay técnicos disponibles, el encargado atiende al próximo cliente de su tipo

			// Obtener posición de cliente de tipo APP
			pthread_mutex_lock(&colaClientes);
			posCliente = obtenerPosicionProximoClienteSegunTipo(0);
			// Obtener tipo del cliente en cuestión
			tipoCliente = listaClientes[posCliente].tipo;
			// El cliente pasa a estar en proceso de ser atendido
			listaClientes[posCliente].atendido = 1;
			pthread_mutex_unlock(&colaClientes);
		}
		else if (numRespReparacionesDisponibles == 0)
		{
			// No hay responsables de reparaciones disponibles, el encargado atiende al próximo cliente de su tipo

			// Obtener posición de cliente de tipo RED
			pthread_mutex_lock(&colaClientes);
			posCliente = obtenerPosicionProximoClienteSegunTipo(1);
			// Obtener tipo del cliente en cuestión
			tipoCliente = listaClientes[posCliente].tipo;
			// El cliente pasa a estar en proceso de ser atendido
			listaClientes[posCliente].atendido = 1;
			pthread_mutex_unlock(&colaClientes);
		}
		else
		{
			// No hay trabajadores ocupados
			continue;
		}

		// Atender al cliente como encargado si hay trabajadores ocupados
		atenderCliente(-1, 1, tipoCliente, posCliente);
	} while (1);
}

void accionesTecnicoDomiciliario()
{
	char *id, *cadena1, *cadena2;
	int i;
	int posicionCliente;

	id = malloc(sizeof(char) * 20);
	cadena1 = malloc(sizeof(char) * 30);
	cadena2 = malloc(sizeof(char) * 20);

	sprintf(id, "tecnico_dom");
	do
	{
		// Comprobamos que el número de solicitudes domiciliarias. Bloqueamos si es menor que 4
		pthread_mutex_lock(&solicitudes);
		while (numSolicitudesDomicilio < NSOLICDOMINECESARIAS)
		{
			// Espera a que el número de solicitudes sea 4
			pthread_cond_wait(&condSolicitudesDomicilio, &solicitudes);
		}
		pthread_mutex_unlock(&solicitudes);

		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Comenzando atención domiciliaria.\n");
		pthread_mutex_unlock(&Fichero);

		// Atendemos cada solicitud
		for (i = 0; i < NSOLICDOMINECESARIAS; i++)
		{
			sleep(1);
			sprintf(cadena1, "Atendido cliente ");
			posicionCliente = obtenerIDClienteAttDom(cadena2);
			//listaClientes[posicionCliente].solicitud = 0;
			strcat(cadena1, cadena2);

			pthread_mutex_lock(&Fichero);
			writeLogMessage(id, cadena1);
			pthread_mutex_unlock(&Fichero);
		}

		// Reseteamos el número de solicitudes a domicilio. Se finaliza la atención domiciliaria.
		pthread_mutex_lock(&solicitudes);
		numSolicitudesDomicilio = 0;
		pthread_mutex_unlock(&solicitudes);

		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Atención domiciliaria finalizada.\n");
		pthread_mutex_unlock(&Fichero);

		// Damos aviso a los que esperaban por atención domiciliaria
		pthread_mutex_lock(&solicitudes);
		for(i = 0; i < NSOLICDOMINECESARIAS; i++)
		{
			pthread_cond_signal(&condSolicitudesDomicilio);
		}
		pthread_mutex_unlock(&solicitudes);

	} while (finalizar == 0);
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

int NClientesRed()
{
	int nred = 0, i;

	for (i = 0; i < NCLIENTES; i++)
	{
		if (listaClientes[i].tipo == 1)
		{
			nred++;
		}
	}

	return nred;
}

void liberaCliente(int posicion)
{
	pthread_mutex_lock(&colaClientes);
	listaClientes[posicion].id = 0;
	listaClientes[posicion].atendido = 0;
	listaClientes[posicion].prioridad = 0;
	listaClientes[posicion].solicitud = 0;
	listaClientes[posicion].tipo = 0;
	pthread_mutex_unlock(&colaClientes);
}

int obtenerPosicionProximoCliente()
{
	// Variables que guardarán los datos del próximo cliente
	int posProxCliente = -1;
	int prioridadProxCliente = -1;
	int tipoProxCliente = 0;
	int idProxCliente = -1;

	int cambiarCliente = 0;

	for (int i = 0; i < NCLIENTES; i++)
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

	for (int i = 0; i < NCLIENTES; i++)
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

void atenderCliente(int tipoTrabajador, int posTrabajador, int tipoCliente, int posCliente)
{
	if (posCliente == -1)
	{
		// No se ha encontrado un cliente al que atender, esperamos 2 segundos
		sleep(2);
	}
	else
	{
		char *idTrabajador, *idCliente, *msg;
		idTrabajador = malloc(sizeof(char) * 20);
		idCliente = malloc(sizeof(char) * 20);
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
			pthread_mutex_lock(&mutexResponsables);
			numID = listaRespReparaciones[posTrabajador].id;
			pthread_mutex_unlock(&mutexResponsables);
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
		writeLogMessage(idCliente, "Va a ser atendido.");
		writeLogMessage(idTrabajador, "Va a atender a un cliente.");
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
			writeLogMessage(idCliente, "Ha terminado de ser atendido. Estaba mal identificado.");
			writeLogMessage(idTrabajador, "Ha terminado de atender a un cliente.");
			pthread_mutex_unlock(&Fichero);

			// Marcar cliente como atendido
			pthread_mutex_lock(&colaClientes);
			listaClientes[posCliente].atendido = 2;
			pthread_mutex_unlock(&colaClientes);
		}
		else if (tipoAtencion == 2)
		{
			// Cliente confundido

			// Atendemos al cliente...
			sleep(calculaAleatorios(1, 2));

			// Indicamos que finaliza la atención y el motivo en el log
			pthread_mutex_lock(&Fichero);
			writeLogMessage(idCliente, "Ha dejado el sistema por confusión.");
			writeLogMessage(idTrabajador, "Ha terminado de atender a un cliente.");
			pthread_mutex_unlock(&Fichero);

			// Marcar cliente como confundido
			pthread_mutex_lock(&colaClientes);
			listaClientes[posCliente].atendido = 3;
			pthread_mutex_unlock(&colaClientes);
		}
		else
		{
			// Cliente con todo en regla

			// Atendemos al cliente...
			sleep(calculaAleatorios(1, 4));

			// Indicamos que finaliza la atención y el motivo en el log
			pthread_mutex_lock(&Fichero);
			writeLogMessage(idCliente, "Ha terminado de ser atendido. Todo en regla.");
			writeLogMessage(idTrabajador, "Ha terminado de atender a un cliente.");
			pthread_mutex_unlock(&Fichero);

			pthread_mutex_lock(&colaClientes);
			listaClientes[posCliente].atendido = 2;
			pthread_mutex_unlock(&colaClientes);
		}
	}
}

int obtenerIDClienteAttDom(char *cadena)
{
	int i, count;
	count = 0;
	i = 0;
	while (i < NCLIENTES)
	{
		if (listaClientes[i].solicitud == 1)
		{
			
			sprintf(cadena, "clired_%d", listaClientes[i].id);
			pthread_mutex_lock(&colaClientes);
			listaClientes[i].solicitud = 0;
			pthread_mutex_unlock(&colaClientes);
			break;
		}
		else
		{
			i++;
		}
	}
	return i;
}

int obtenerNumTecnicosDisponibles()
{
	int tdisponibles = 0;

	pthread_mutex_lock(&mutexTecnicos);

	for (int i = 0; i < NTECNICOS; i++)
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

	pthread_mutex_lock(&mutexResponsables);

	for (int i = 0; i < NRESPREPARACIONES; i++)
	{
		if (listaRespReparaciones[i].disponible == 1)
		{
			rdisponibles++;
		}
	}

	pthread_mutex_unlock(&mutexResponsables);

	return rdisponibles;
}

void printWelcome()
{
	printf("=====================================================================\n");
	printf("=======BIENVENIDO AL SISTEMA DE GESTIÓN DE AVERÍAS luZECita==========\n");
	printf("=====================================================================\n");

}
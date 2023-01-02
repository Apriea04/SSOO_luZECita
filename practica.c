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

// TODO: Crear variable para llevar a cabo salida controlada

/**DECLARACIONES GLOBALES*/

// Mutex
pthread_mutex_t Fichero, colaClientes, mutexTrabajadores, solicitudes;

// Variables condición
pthread_cond_t condSolicitudesDomicilio;

// Contadores
int contadorApp, contadorRed, numSolicitudesDomicilio;

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
void nuevoCliente(int tipo);

void accionesCliente(int posicion);

void accionesTecnico(int tipoTrabajador, int posTrabajador);

void accionesEncargado();

void accionesTecnicoDomiciliario();

/**FUNCIONES*/

/**
 * Escribe un mensaje nuevo en el fichero registroTiempos.log.
 * El mensaje estará compuesto por la fecha y la hora, el identificador del hilo que lo ejecuta y el mensaje.
 */
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

/**
 * Calcula un número aleatorio entre el mínimo y el máximo espeficicados.
 * Ambos números se incluyen en el cálculo.
 */
int calculaAleatorios(int min, int max)
{
	return rand() % (max - min + 1) + min;
}

/**
 * Devuelve el número de clientes de tipo red
 */
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

/**
 * Devuelve la posición de un cliente en la lista de clientes.
 * La primera posición es la 0
 * Precondición: el cliente debe estar en la lista
 */
int obtenerPosicion(struct Cliente *buscado)
{
	int i = 0, encontrado = 0;
	pthread_mutex_lock(&colaClientes);
	while (encontrado == 0 && i < NCLIENTES)
	{
		// Un cliente es identificado por su id y tipo.
		if (listaClientes[i].id == buscado->id && listaClientes[i].tipo == buscado->tipo)
		{
			encontrado = 1;
		}
		i++;
	}
	pthread_mutex_unlock(&colaClientes);
	if (encontrado)
	{
		return i;
	}
	else
	{
		return -1;
	}
}

/**
 * Libera el cliente del vector de clientes.
 */
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

/**
 * Obtiene al próximo cliente que debe ser atendido,
 * tendiendo en cuenta su tipo, prioridad y tiempo esperado.
 */
int obtenerPosicionProximoCliente()
{
	// Variables que guardarán los datos del próximo cliente
	int posProxCliente = -1;
	int prioridadProxCliente = -1;
	int tipoProxCliente = 0;
	int idProxCliente = -1;

	int cambiarCliente = 0;

	pthread_mutex_lock(&colaClientes);
	for (int i = 0; i < NCLIENTES; i++)
	{
		cambiarCliente = 0;
		if (listaClientes[i].id != 0)
		{
			// Es un cliente, no está vacío
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
	pthread_mutex_unlock(&colaClientes);

	return posProxCliente;
}

/**
 * Obtiene al próximo cliente de un tipo específico que debe
 * ser atendido teniendo en cuenta su prioridad y tiempo esperado.
 *
 * tipoCliente (int): tipo de cliente que se va a buscar
 */
int obtenerPosicionProximoClienteSegunTipo(int tipoCliente)
{
	// Variables que guardarán los datos del próximo cliente
	int posProxCliente = -1;
	int prioridadProxCliente = -1;
	int idProxCliente = -1;

	int cambiarCliente = 0;

	pthread_mutex_lock(&colaClientes);
	for (int i = 0; i < NCLIENTES; i++)
	{
		cambiarCliente = 0;
		if (listaClientes[i].id != 0)
		{
			// Es un cliente, no está vacío
			if (listaClientes[i].tipo != tipoCliente)
			{
				// El cliente actual no tiene el tipo de cliente buscado, lo omitimos
				continue;
			}
			else
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
	pthread_mutex_unlock(&colaClientes);

	return posProxCliente;
}

/**
 * Lleva a cabo el proceso de atención de un técnico a un cliente
 *
 * tipoTrabajador (int): tipo de técnico que está atendiendo al cliente
 * posTrabajador (int): posición del técnico en su respectiva lista
 * tipoCliente (int): tipo de cliente
 * posCliente (int): posición del cliente en la lista de clientes
 */
void atenderCliente(int tipoTrabajador, int posTrabajador, int tipoCliente, int posCliente)
{
	// TODO: Agregar mensajes de técnicos a los logs usando tipoTecnico y posTecnico y aumentar clientes atendidos

	if (posCliente == -1)
	{
		// No se ha encontrado un cliente al que atender, esperamos 2 segundos
		sleep(2);
	}
	else
	{
		// Marcar al cliente como en proceso de atención
		pthread_mutex_lock(&colaClientes);
		listaClientes[posCliente].atendido = 1;
		pthread_mutex_unlock(&colaClientes);

		char *idTrabajador, *idCliente, *msg;
		idTrabajador = malloc(sizeof(char)*20);
		idCliente = malloc(sizeof(char)*20);
		msg = malloc(sizeof(char)*100);

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
			pthread_mutex_lock(&mutexTrabajadores);
			numID = listaTecnicos[posTrabajador].id;
			pthread_mutex_unlock(&mutexTrabajadores);
			sprintf(idTrabajador, "tecnico_%d", numID);
		}
		else if (tipoTrabajador == 1)
		{
			// Definir id de responsable de reparaciones
			pthread_mutex_lock(&mutexTrabajadores);
			numID = listaRespReparaciones[posTrabajador].id;
			pthread_mutex_unlock(&mutexTrabajadores);
			sprintf(idTrabajador, "resprep_%d", numID);
		}

		// Definir id del cliente según su tipo
		if (tipoCliente == 0)
		{
			// Definir id de cliente de red
			pthread_mutex_lock(&colaClientes);
			numID = listaClientes[posCliente].id;
			pthread_mutex_unlock(&colaClientes);
			sprintf(idCliente, "cliapp_%d", numID);
		}
		else
		{
			// Definir id de cliente de app
			pthread_mutex_lock(&colaClientes);
			numID = listaClientes[posCliente].id;
			pthread_mutex_unlock(&colaClientes);
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

/**
 * Escribe en el puntero el identificador del próximo cliente que recibió atención domiciliaria y
 * establece la solicitud a 0 del cliente la solicitó.
 */
void obtenerIDClienteAttDom(char *cadena)
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
}

// TODO: Algunas funciones se han escrito al final en vez de aquí, sería conveniente moverlas.

/**MANEJADORAS DE SEÑAL*/

void handlerClienteApp(int sig)
{
	nuevoCliente(0);
}

void handlerClienteRed(int sig)
{
	nuevoCliente(1);
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
	accionesTecnico(0, index);
	printf("Técnico %d\n", index);
	free(arg);
}

void *Responsable(void *arg)
{
	int index = *(int *)arg;
	accionesTecnico(1, index);
	printf("Responsable %d\n", index);
	free(arg);
}

void *Encargado(void *arg)
{
	int index = *(int *)arg;
	accionesEncargado();
	printf("Encargado %d\n", index);
	free(arg);
}

void *AtencionDomiciliaria(void *arg)
{
	int index = *(int *)arg;
	accionesTecnicoDomiciliario();
	printf("Atención domiciliaria %d\n", index);
	free(arg);
}

void *Cliente(void *arg)
{
	printf("Cliente nuevo\n");
	int valor = *(int *)arg;
	accionesCliente(valor);
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
	pthread_mutex_init(&mutexTrabajadores, NULL);
	pthread_mutex_init(&solicitudes, NULL);

	// Inicialización de variables condición
	pthread_cond_init(&condSolicitudesDomicilio, NULL);

	// Inicialización de contadores
	contadorApp = 0;
	contadorRed = 0;
	numSolicitudesDomicilio = 0;

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
		listaTecnicos[i].numClientesAtendidos = 0;
	}

	// Inicialización de técnicos
	for (i = 0; i < NTECNICOS; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&tecnicos[i], NULL, &Tecnico, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de técnico.");
			return -1;
		}

		// Agregar a lista de técnicos
		listaTecnicos[i].id = i + 1;
	}

	// Inicialización de lista de reponsables de reparaciones
	listaRespReparaciones = (struct Trabajador *)malloc(NRESPREPARACIONES * sizeof(struct Trabajador));
	for (i = 0; i < NRESPREPARACIONES; i++)
	{
		listaRespReparaciones[i].id = 0;
		listaRespReparaciones[i].numClientesAtendidos = 0;
	}

	// Inicialización de responsables de reparaciones
	for (i = 0; i < NRESPREPARACIONES; i++)
	{
		int *index = malloc(sizeof(int));
		*index = i + 1;
		if (pthread_create(&respReparaciones[i], NULL, &Responsable, index) != 0)
		{
			perror("[ERROR] Error al crear hilo de responsable de reparaciones.");
			return -1;
		}

		// Agregar a lista de responsables de reparaciones
		listaTecnicos[i].id = i + 1;
	}

	// Inicialización de encargados
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

	// Inicialización de técnicos de atención domiciliaria
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

	while (1)
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
	pthread_mutex_destroy(&mutexTrabajadores);
	pthread_mutex_destroy(&solicitudes);

	return 0;
}

// GUILLERMO
/**
 * Introduce un cliente en la cola de clientes cuando se recibe una señal.
 * Cuando aparece un cliente, lo introduce en la primera posición libre que encuentre (Cuando  id==0).
 * Si no hay posiciones libres, no se hace nada y se ignora el cliente.
 *
 * @param tipo
 */
void nuevoCliente(int tipo)
{
	pthread_mutex_lock(&colaClientes);
	// Busca posición libre dentro del array (id=0)
	int i = 0;
	while (i < NCLIENTES)
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

// ÁLVARO

/**
 * Recibe un puntero a su estructura cliente de la cola de clientes
 * @param cliente
 */
void accionesCliente(int posicion)
{
	struct Cliente *cliente = malloc(sizeof(struct Cliente));
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
				pthread_mutex_unlock(&solicitudes);
				pthread_cond_signal(&condSolicitudesDomicilio);
			}
			else
			{
				pthread_mutex_unlock(&solicitudes);
			}

			pthread_mutex_lock(&colaClientes);
			int estadoSolicitud = listaClientes[posicion].solicitud;
			pthread_mutex_unlock(&colaClientes);

			while (estadoSolicitud != 0)
			{
				pthread_cond_wait(&condSolicitudesDomicilio, &solicitudes);
				pthread_mutex_lock(&colaClientes);
				int estadoSolicitud = listaClientes[posicion].solicitud;
				pthread_mutex_unlock(&colaClientes);
			}

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

// DANIEL
/**
 * LLeva a cabo las funciones de un técnico o
 * responsable de reparaciones
 *
 * tipoTrabajador (int): entero que representa si un trabajador
 * 	                  se encarga de la app o de la red
 * posTrabajador (int): entero con la posición en la lista del trabajador
 * @param tipoTrabajador
 * @param posTrabajador
 */
void accionesTecnico(int tipoTrabajador, int posTrabajador)
{
	int posCliente;
	int tipoCliente;
	int atendido;

	// Bucle en el que el trabajador va atendiendo a los clientes que le lleguen
	do
	{
		// Comprobar si es necesario un descanso según el tipo
		int numClientesAtendidos;
		if (tipoTrabajador == 0)
		{
			// Técnico
			pthread_mutex_lock(&mutexTrabajadores);
			numClientesAtendidos = listaTecnicos[posTrabajador].numClientesAtendidos;
			pthread_mutex_unlock(&mutexTrabajadores);

			// El técnico descansa 5 segundos por cada 5 clientes
			if (numClientesAtendidos % 5 == 0)
			{
				sleep(5);
			}
		}
		else if (tipoTrabajador == 1)
		{
			// Responsable de reparaciones
			pthread_mutex_lock(&mutexTrabajadores);
			numClientesAtendidos = listaRespReparaciones[posTrabajador].numClientesAtendidos;
			pthread_mutex_unlock(&mutexTrabajadores);

			// El responsable de reparaciones descansa 6 segundos por cada 6 clientes
			if (numClientesAtendidos % 6 == 0)
			{
				sleep(6);
			}
		}

		// Obtener posición del cliente a atender según el tipo de trabajador
		posCliente = obtenerPosicionProximoClienteSegunTipo(tipoTrabajador);
		// Obtener tipo del cliente en cuestión
		pthread_mutex_lock(&colaClientes);
		tipoCliente = listaClientes[posCliente].tipo;
		atendido = listaClientes[posCliente].atendido;
		pthread_mutex_unlock(&colaClientes);

		// Atender al cliente
		if(atendido == 0)
		{
			atenderCliente(tipoTrabajador, posTrabajador, tipoCliente, posCliente);
		}
		

		// Aumentar número de clientes atendidos
		if (tipoTrabajador == 0)
		{
			// Aumentar número de clientes atendidos a técnico
			pthread_mutex_lock(&mutexTrabajadores);
			listaTecnicos[posTrabajador].numClientesAtendidos = listaTecnicos[posTrabajador].numClientesAtendidos + 1;
			pthread_mutex_unlock(&mutexTrabajadores);
		}
		else if (tipoTrabajador == 1)
		{
			// Aumentar número de clientes atendidos a responsable de reparaciones
			pthread_mutex_lock(&mutexTrabajadores);
			listaRespReparaciones[posTrabajador].numClientesAtendidos = listaRespReparaciones[posTrabajador].numClientesAtendidos + 1;
			pthread_mutex_unlock(&mutexTrabajadores);
		}
	} while (1);
}

// MARIO
/**
 * LLeva a cabo las funciones de un encargado
 */
void accionesEncargado()
{
	int posCliente;
	int tipoCliente; 
	int atendido;

	// Bucle en el que el encargado va atendiendo a los clientes que le lleguen
	do
	{
		// Obtener el siguiente cliente a atender según las reglas establecidas
		posCliente = obtenerPosicionProximoCliente();
		// Obtener tipo del cliente en cuestión
		pthread_mutex_lock(&colaClientes);
		tipoCliente = listaClientes[posCliente].tipo;
		atendido = listaClientes[posCliente].atendido;
		pthread_mutex_unlock(&colaClientes);

		// Atender al cliente como encargado
		if(atendido==0){
			atenderCliente(-1, 1, tipoCliente, posCliente);
		}
		
	} while (1);
}

void accionesTecnicoDomiciliario()
{
	char *id, *cadena1, *cadena2;
	int i;

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
			obtenerIDClienteAttDom(cadena2);
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
		pthread_cond_signal(&condSolicitudesDomicilio);
		pthread_mutex_unlock(&solicitudes);

	} while (1);
}

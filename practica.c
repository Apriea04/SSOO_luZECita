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

/**DECLARACIONES GLOBALES*/

// Mutex
pthread_mutex_t Fichero, colaClientes, solicitudes;

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

// Listas de hilos trabajadores
pthread_t tecnicos[NTECNICOS];
pthread_t respReparaciones[NRESPREPARACIONES];
pthread_t encargados[NENCARGADOS];
pthread_t tecAttDomiciliaria[NTECDOMICILIARIA];

// Lista de clientes
struct Cliente listaClientes[NCLIENTES];

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

int calculaAleatorios(int min, int max)
{
	return rand() % (max - min + 1) + min;
}

// Devuelve el número de clientes de red
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
void liberaCliente(struct Cliente *cliente)
{
	pthread_mutex_lock(&colaClientes);
	cliente->id = 0;
	cliente->atendido = 0;
	cliente->prioridad = 0;
	cliente->solicitud = 0;
	cliente->tipo = 0;
	pthread_mutex_unlock(&colaClientes);
}

/**MANEJADORAS DE SEÑAL*/

void handlerClienteApp(int sig)
{
	printf("Hola SIGUSR1\n");
	nuevoCliente(0);
}

void handlerClienteRed(int sig)
{
	printf("Hola SIGUSR2\n");
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

void *ClienteApp(void *arg)
{
	printf("Cliente App\n");
}

void *ClienteRed(void *arg)
{
	printf("Cliente Red\n");
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

	for (i = 0; i < NCLIENTES; i++)
	{
		listaClientes[i].id = 0;
		listaClientes[i].prioridad = 0;
		listaClientes[i].atendido = 0;
		listaClientes[i].solicitud = 0;
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
		pause();
	}

	// IMPORTANTE: No sé si esto es necesario, conviene revisarlo posteriormente - Joins de todos los hilos
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

	// Eliminar los mutex al salir
	pthread_mutex_destroy(&Fichero);
	pthread_mutex_destroy(&colaClientes);
	pthread_mutex_destroy(&solicitudes);

	return 0;
}

// GUILLERMO
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

			// Creamos un hilo cliente y lo inicializamos
			pthread_t cliente;
			char *id;
			if (listaClientes[i].tipo == 0)
			{
				pthread_create(&cliente, NULL, &ClienteApp, NULL);
				sprintf(id, "cliapp_%d", listaClientes[i].id);
				pthread_mutex_lock(&Fichero);
				writeLogMessage(id, "Nuevo cliente de tipo APP ha entrado en la cola.");
				pthread_mutex_unlock(&Fichero);
			}
			else
			{
				pthread_create(&cliente, NULL, &ClienteRed, NULL);
				sprintf(id, "clired_%d", listaClientes[i].id);
				pthread_mutex_lock(&Fichero);
				writeLogMessage(id, "Nuevo cliente de tipo RED ha entrado en cola.");
				pthread_mutex_unlock(&Fichero);
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
 */
void accionesCliente(struct Cliente *cliente)
{
	char *id, *msg;
	int seVa = 0; // en principio, el cliente no se va.

	pthread_mutex_lock(&colaClientes); // TODO duda ¿Proteger?
	int tipo = cliente->tipo;
	pthread_mutex_unlock(&colaClientes);

	if (tipo == 0)
	{
		// Cliente app
		sprintf(id, "clieapp_%d", cliente->id);
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Cliente de tipo APP acaba de entrar al sistema.");
		pthread_mutex_unlock(&Fichero);
	}
	else
	{
		sprintf(id, "clired_%d", cliente->id);
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Cliente de tipo RED acaba de entrar al sistema.");
		pthread_mutex_unlock(&Fichero);
	}

	// Calculamos si se va porque la aplicación es difícil:
	if (calculaAleatorios(0, 10) < 2)
	{
		// Encontró la aplicación dificil
		pthread_mutex_lock(&Fichero);
		writeLogMessage(id, "Encontró la aplicación difícil y se fue.");
		pthread_mutex_unlock(&Fichero);
	}

	int atendido = 0; // En principio piensa que no está siendo atendido
	int tiempoEsperando = 0;
	do // Mientras que no sea atendido, comprueba cada 2 segundos
	{
		if (seVa)
		{
			// Libero su espacio de la cola
			liberaCliente(cliente);

			// Se va
			pthread_exit(0);
		}
		// ¿Estoy atendido?
		pthread_mutex_lock(&colaClientes);
		atendido = cliente->atendido;
		pthread_mutex_unlock(&colaClientes);

		if (!atendido)
		{
			// ¿Se cansó de esperar?
			if (tiempoEsperando % 8 == 0 && calculaAleatorios(0, 100) <= 20)
			{
				sprintf(msg, "Se cansó de esperar tras %d segundos", tiempoEsperando);

				pthread_mutex_lock(&Fichero);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&Fichero);

				seVa = 1;
			}
			else if (calculaAleatorios(0, 100) <= 5)
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
	} while (!atendido);

	// Ahora ya esta siendo atendido

	while (atendido == 1)
	{
		// Estoy siendo atendido
		sleep(2);

		pthread_mutex_lock(&colaClientes);
		atendido = cliente->atendido;
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

				if (sol > 4)
				{
					sleep(3);
				}
			} while (sol > 4);

			// numSolicitudesDomicilio <= 4

			pthread_mutex_lock(&logFile);
			writeLogMessage(id, "Esperando a ser atendido en domicilio.");
			pthread_mutex_unlock(&logFile);

			pthread_mutex_lock(&colaClientes);
			cliente->solicitud = 1;
			pthread_mutex_unlock(&colaClientes);

			pthread_mutex_lock(&solicitudes);
			numSolicitudesDomicilio += 1;
			if (numSolicitudesDomicilio == 4)
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
			int estadoSolicitud = cliente->solicitud;
			pthread_mutex_unlock(&colaClientes);

			while (estadoSolicitud != 0)
			{
				pthread_cond_wait(&condSolicitudesDomicilio);
				pthread_mutex_lock(&colaClientes);
				int estadoSolicitud = cliente->solicitud;
				pthread_mutex_unlock(&colaClientes);
			}

			pthread_mutex_lock(&logFile);
			writeLogMessage(id, "Recibió atención domiciliaria.");
			pthread_mutex_unlock(&logFile);
		}
	}

	// El cliente se va
	liberaPosicion(cliente);
	pthread_mutex_lock(&logFile);
	writeLogMessage(id, "Se va tras haber sido atendido");
	pthread_mutex_unlock(&logFile);
	pthread_exit(0);
}

// DANIEL
void accionesTecnico()
{
}

// MARIO
void accionesEncargado()
{
	int customer = -1; // Cliente encontrado.
	int tipoAtencion;
	char *id, *msg;

	do
	{
		pthread_mutex_lock(&colaClientes);

		// Busca el cliente que atendera segun su tipo, prioridad y tiempo de espera.
		for (int i = 0; i < NCLIENTES; i++)
		{

			// Si el cliente es actual no es vacio y no esta atendio se procede a comparar
			if (listaClientes[i].id != 0 && listaClientes[i].atendido == 0)
			{
				// Si no hay ningún cliente seleccionado se añade su posición
				if (customer == -1)
				{
					customer = i;
				}
				// Si la posición del cliente es menor que la posición final se procede
				else if (customer < NCLIENTES - 1)
				{
					// Si el cliente actual de la lista es de tipo RED procedemos
					if (listaClientes[i].tipo == 1)
					{
						// Si el cliente seleccionado es de tipo APP se cambia su posición con el cliente actual
						if (listaClientes[customer].tipo == 0)
						{
							customer = i;
						}
						// Si el cliente actual tiene mayor prioridad que el cliente seleccionado, el actual pasa a ser el seleccionado
						else if (listaClientes[i].prioridad > listaClientes[customer].prioridad)
						{
							customer = i;
						}
						// Si el cliente actual y el seleccionado tienen la misma prioridad se procede
						else if (listaClientes[i].prioridad == listaClientes[customer].prioridad)
						{
							// Si la id del cliente actual es menor que la del cliente seleccionado, el actual pasa a ser el seleccionado
							if (listaClientes[i].id < listaClientes[customer].id)
							{
								customer = i;
							}
						}
						// Si el cliente actual de la lista es de tipo APP procedemos
						else
						{
							// Si el cliente seleccionado es de tipo APP procedemos
							if (listaClientes[customer].tipo == 0)
							{
								// Si el cliente actual tiene mayor prioridad que el cliente seleccionado, el actual pasa a ser el seleccionado
								if (listaClientes[i].prioridad > listaClientes[customer].prioridad)
								{
									customer = i;
								}
								// Si el cliente actual y el seleccionado tienen la misma prioridad se procede
								else if (listaClientes[i].prioridad == listaClientes[customer].prioridad)
								{
									// Si la id del cliente actual es menor que la del cliente seleccionado, el actual pasa a ser el seleccionado
									if (listaClientes[i].id < listaClientes[customer].id)
									{
										customer = i;
									}
								}
							}
						}
					}
				}
			}
		}

		/*
	// Alvaro idea
	pthread_mutex_lock(&colaClientes);
	int posCliente = -1, prioridad = -1,
		tipo = 0, id = -1, cambiamosCliente; // El encargado prefiere tipo
	for (int i = 0; i < NCLIENTES; i++)
	{
		cambiamosCliente = 0;
		if (listaClientes.id != 0)
		{
			// Es un cliente, no está vacío
			if (listaClientes[i].tipo == 1 && tipo == 0)
			{
				// Tenemos un cliente de tipo preferido frente a uno no preferido. Lo cojemos sí o sí
				// Actualizamos los campos
				cambiamosCliente = 1;
			}
			else if (listaClientes[i].tipo == tipo && tipo == 1 || listaClientes[i].tipo == tipo && tipo == 0)
			{
				// Tenemos dos clientes del mismo tipo
				if (listaClientes[i].prioridad > prioridad)
				{
					// Tiene más prioridad, luego lo cogemos
					// Actualizamos los campos
					cambiamosCliente = 1;
				}
				else if (listaClientes[i].prioridad == prioridad)
				{
					// Tienen la misma prioridad, cogemos el de menor id
					if (listaClientes[i].id < id)
					{
						// Actualizamos los campos
						cambiamosCliente = 1;
					}
				}
			}
		}
		if (cambiamosCliente == 1)
		{
			// Actualizamos los campos
			posCliente = i;
			prioridad = listaClientes[i].prioridad;
			tipo = listaClientes[i].tipo;
			id = listaClientes[i].id;
		}
	}
	pthread_mutex_unlock(&colaClientes);
	// Fin idea
	*/

		if (customer == -1)
		{
			pthread_mutex_unlock(&colaClientes);
			sleep(3);
		}
		else
		{
			listaClientes[customer].atendido = 1;
			pthread_mutex_unlock(&colaClientes);

			struct Cliente current = listaClientes[customer];

			// Calcular tipo de atencion
			tipoAtencion = calculaAleatorios(1, 10);

			if (tipoAtencion == 1)
			{
				// Mal Identificados

				// Idicamos que comienza la atención del cliente
				if (current.tipo == 1)
				{
					// Cliente red
					sprintf(id, "clired_%d", current.id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "Cliente de tipo RED va a ser atendido.");
					pthread_mutex_unlock(&Fichero);
				}
				else
				{
					// Cliente app
					sprintf(id, "clieapp_%d", listaClientes[customer].id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "Cliente de tipo APP va a ser antendido");
					pthread_mutex_unlock(&Fichero);
				}

				sleep(calculaAleatorios(2, 6));

				// Indicamos que finaliza la atención y el motivo
				if (current.tipo == 1)
				{
					// Cliente red
					sprintf(id, "clired_%d", current.id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "A finalizado la atención del cliente.");
					writeLogMessage(id, "El cliente estaba mal identificado.");
					pthread_mutex_unlock(&Fichero);
				}
				else
				{
					// Cliente app
					sprintf(id, "clieapp_%d", listaClientes[customer].id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "A finalizado la antención del cliente");
					writeLogMessage(id, "El cliente estaba mal identificado.");
					pthread_mutex_unlock(&Fichero);
				}

				pthread_mutex_lock(&colaClientes);
				listaClientes[customer].atendido = 2;
				pthread_mutex_unlock(&colaClientes);
			}
			else if (tipoAtencion == 2)
			{
				// Confundidos

				// Indicamos que comienza la atención del cliente
				if (current.tipo == 1)
				{
					// Cliente red
					sprintf(id, "clired_%d", current.id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "Cliente de tipo RED va a ser atendido.");
					pthread_mutex_unlock(&Fichero);
				}
				else
				{
					// Cliente app
					sprintf(id, "clieapp_%d", listaClientes[customer].id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "Cliente de tipo APP va a ser antendido");
					pthread_mutex_unlock(&Fichero);
				}

				sleep(calculaAleatorios(1, 2));

				// Indicamos que finaliza la atención y el motivo
				if (current.tipo == 1)
				{
					// Cliente red
					sprintf(id, "clired_%d", listaClientes[customer].id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "Cliente de tipo RED deja el sistema debido a confusión");
					pthread_mutex_unlock(&Fichero);
				}
				else
				{
					// Cliente app
					sprintf(id, "clieapp_%d", listaClientes[customer].id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "Cliente de tipo APP deja el sistema debido a confusión");
					pthread_mutex_unlock(&Fichero);
				}

				pthread_mutex_lock(&colaClientes);
				listaClientes[customer].atendido = 3;
				pthread_mutex_unlock(&colaClientes);
			}
			else
			{
				// Todo en regla

				// Idicamos que comienza la atención del cliente
				if (current.tipo == 1)
				{
					// Cliente red
					sprintf(id, "clired_%d", current.id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "Cliente de tipo RED va a ser atendido.");
					pthread_mutex_unlock(&Fichero);
				}
				else
				{
					// Cliente app
					sprintf(id, "clieapp_%d", listaClientes[customer].id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "Cliente de tipo APP va a ser antendido");
					pthread_mutex_unlock(&Fichero);
				}

				sleep(calculaAleatorios(1, 4));

				// Indicamos que finaliza la atención y el motivo
				if (current.tipo == 1)
				{
					// Cliente red
					sprintf(id, "clired_%d", current.id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "A finalizado la atención del cliente.");
					writeLogMessage(id, "El cliente tiene todo en regla.");
					pthread_mutex_unlock(&Fichero);
				}
				else
				{
					// Cliente app
					sprintf(id, "clieapp_%d", listaClientes[customer].id);
					pthread_mutex_lock(&Fichero);
					writeLogMessage(id, "A finalizado la antención del cliente");
					writeLogMessage(id, "El cliente tiene todo en regla.");
					pthread_mutex_unlock(&Fichero);
				}

				pthread_mutex_lock(&colaClientes);
				listaClientes[customer].atendido = 2;
				pthread_mutex_unlock(&colaClientes);
			}
		}
	} while (1);
}

void accionesTecnicoDomiciliario()
{
}

/**
 * @file init_master.h
 * @brief Header principal del módulo Master
 * 
 * Este archivo define la estructura principal del Master y las funciones de inicialización
 * y destrucción del sistema. El Master es responsable de:
 * - Gestionar conexiones de Query Controls y Workers
 * - Planificar la ejecución de queries según algoritmos configurables (FIFO, Prioridades)
 * - Implementar aging para evitar starvation
 * - Manejar desalojos por prioridad
 * - Coordinar la comunicación entre componentes del sistema
 */

#ifndef INIT_MASTER_H
#define INIT_MASTER_H

#include <pthread.h>
#include <commons/log.h>

// Forward declarations (para evitar inclusiones circulares)
// Se utilizan punteros a estas estructuras en t_master
typedef struct query_table t_query_table;
typedef struct worker_table t_worker_table;

// Estructura principal del Master
typedef struct master {
    // Tablas principales
    t_query_table *queries_table; // Tabla de control de queries
    t_worker_table *workers_table; // Tabla de control de workers

    // Datos de configuración
    char *ip; // IP del Master
    char *port; // Puerto del Master
    int aging_interval; // Intervalo de "envejecimiento"
    char *scheduling_algorithm; // Algoritmo de planificación
    int multiprogramming_level; // Nivel de multiprogramación (Workers conectados)

    // Hilos principales
    pthread_t aging_thread; // Hilo de envejecimiento
    pthread_t scheduling_thread; // Hilo de planificación

    // Logger
    t_log *logger;
} t_master;


/**
 * @brief Inicializa una nueva instancia del Master con la configuración especificada
 * 
 * Esta función:
 * - Alloca memoria para la estructura principal del Master
 * - Inicializa las tablas de queries y workers
 * - Configura los parámetros del sistema
 * - Prepara los mutex y estructuras de sincronización
 * - Registra la inicialización en el logger
 * 
 * @param ip Dirección IP en la que el Master escuchará conexiones (no puede ser NULL)
 * @param port Puerto en el que el Master escuchará conexiones (no puede ser NULL)
 * @param aging_interval Intervalo en milisegundos para el aging (debe ser > 0)
 * @param scheduling_algorithm Algoritmo a usar: "FIFO" o "PRIORITY" (no puede ser NULL)
 * @param logger Logger configurado para el módulo Master (no puede ser NULL)
 * 
 * @return Puntero a la estructura t_master inicializada, o NULL en caso de error
 * 
 * @note La memoria allocada debe liberarse posteriormente con destroy_master()
 * @note En caso de error, se registra en el logger y se libera memoria parcialmente allocada
 */
t_master* init_master(char *ip, char *port, int aging_interval, char *scheduling_algorithm, t_log *logger);

/**
 * @brief Libera toda la memoria y recursos asociados a una instancia del Master
 * 
 * Esta función:
 * - Libera las tablas de queries y workers
 * - Libera los strings de configuración
 * - Destruye los mutex y estructuras de sincronización
 * - Libera la memoria de la estructura t_master
 * 
 * @param master Puntero a la estructura t_master a destruir (puede ser NULL)
 */
void destroy_master(t_master *master);


#endif
#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <commons/log.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils/filesystem_utils.h>
#include <globals/globals.h>
#include "file_locks.h"

#define TEST_MOUNT_POINT "/tmp/storage_test"
#define TEST_BLOCK_SIZE 128
#define TEST_FS_SIZE 4096

/**
 * Crea una carpeta temporaria para operaciones del filesystem
 *
 * @return 0 en caso de exito, -1 si falla
 */
int create_test_directory(void);

/**
 * Elimina el carpeta temporario de prueba y todo su contenido
 *
 * @return 0 en caso de exito, -1 si falla
 */
int cleanup_test_directory(void);

/**
 * Crea un logger mockeado
 *
 * @return t_log* instancia del logger o NULL si falla
 */
t_log *create_test_logger(void);

/**
 * Destruye un logger de prueba
 *
 * @param logger Logger a destruir
 */
void destroy_test_logger(t_log *logger);

/**
 * Verifica si un archivo existe
 *
 * @param file_path Path del archivo a verificar
 * @return 1 si el archivo existe, 0 si no existe
 */
int file_exists(const char *file_path);

/**
 * Verifica si una carpeta existe
 *
 * @param dir_path Path del carpeta a verificar
 * @return 1 si el carpeta existe, 0 si no existe
 */
int directory_exists(const char *dir_path);

/**
 * Lee el contenido de un archivo en un buffer
 *
 * @param file_path Path del archivo a leer
 * @param buffer Buffer para almacenar el contenido
 * @param max_size Tamaño maximo a leer
 * @return Numero de bytes leidos, -1 si hay error
 */
int read_file_contents(const char *file_path, char *buffer, size_t max_size);

/**
 * Verifica si una carpeta esta vacia
 *
 * @param dir_path Path del carpeta a verificar
 * @return 1 si esta vacio, 0 si no esta vacio, -1 si hay error
 */
int is_directory_empty(const char *dir_path);

/**
 * Cuenta el numero de archivos en una carpeta
 *
 * @param dir_path Path del carpeta
 * @return Numero de archivos, -1 si hay error
 */
int count_files_in_directory(const char *dir_path);

/**
 * Verifica que un archivo tiene el tamaño correcto
 *
 * @param file_path Path del archivo a verificar
 * @param expected_size Tamaño esperado en bytes
 * @return 1 si el tamaño coincide, 0 si no coincide, -1 si hay error
 */
int verify_file_size(const char *file_path, size_t expected_size);

/**
 * Verifica si dos archivos estan hardlinkeados
 *
 * @param file1_path Path del primer archivo
 * @param file2_path Path del segundo archivo
 * @return 1 si estan unidos por hard link, 0 si no, -1 si hay error
 */
int files_are_hardlinked(const char *file1_path, const char *file2_path);

/**
 * Crea un archivo superblock.config de prueba con valores predeterminados
 *
 * @param mount_point Path donde crear el superblock
 * @return 0 en caso de exito, -1 si falla
 */
int create_test_superblock(const char *mount_point);

/**
 * Crea un archivo de configuración para el módulo Storage.
 * 
 * @param storage_ip Dirección IP del servicio de almacenamiento.
 * @param storage_port Puerto del servicio de almacenamiento.
 * @param fresh_start Flag de inicio limpio.
 * @param mount_point Ruta base donde se creará el archivo de configuración.
 * @param operation_delay Tiempo de retardo en milisegundos para operaciones.
 * @param block_access_delay Tiempo de retardo en milisegundos para el acceso a bloques.
 * @param log_level Nivel de log deseado.
 * @return int 0 si la configuración se creó exitosamente.
 * @return int -1 si alguno de los parámetros de cadena es nulo.
 * @return int -2 si falla la apertura o escritura del archivo.
 */
int create_test_storage_config(
    char *storage_ip, 
    char *storage_port, 
    char *fresh_start,
    char *mount_point,
    int operation_delay,
    int block_access_delay,
    char *log_level
);

/**
 * Inicializa los bloques lógicos de un archivo simulado.
 * Crea hardlinks a un bloque físico inicial (block0000.dat).
 * 
 * @param name Nombre del archivo.
 * @param tag Etiqueta del archivo.
 * @param numb_blocks Número de bloques lógicos a inicializar para el archivo.
 * @param mount_point Punto de montaje base.
 * @return int 0 si los bloques lógicos se inicializaron correctamente.
 * @return int -1 si falla la creación recursiva del directorio.
 * @return int -2 si falla la creación de algún hardlink al bloque físico.
 */
int init_logical_blocks(const char *name, const char *tag, int numb_blocks, const char *mount_point);

/**
 * Crea el archivo de metadatos para un archivo de prueba.
 * 
 * @param name Nombre del archivo.
 * @param tag Etiqueta asociada al archivo.
 * @param numb_blocks Número de bloques que debe contener el archivo.
 * @param blocks_array_str Array de IDs de bloques (ej: "[1,2,3,4]").
 * @param status Estado del archivo: "WORK_IN_PROGRESS" o "COMMITTED".
 * @param mount_point Punto de montaje base.
 * @return int 0 si el archivo de metadatos se creó exitosamente.
 * @return int -1 si el número de bloques solicitado excede el tamaño del FS de prueba (TEST_FS_SIZE).
 * @return int -2 si el número de bloques solicitado no coincide con la longitud del array de bloques proporcionado.
 * @return int -3 si falla la apertura o escritura del archivo de metadatos.
 */
int create_test_metadata(const char *name, const char *tag, int numb_blocks, char *blocks_array_str, char *status, char *mount_point);

/**
 * Verifica si un archivo ha sido desbloqueado correctamente en el diccionario de archivos abiertos.
 * 
 * @note Esta función asume la existencia de las variables globales g_storage_open_files_dict_mutex 
 * y g_open_files_dict (un diccionario que almacena t_file_mutex*).
 * 
 * @param name Nombre del archivo.
 * @param tag Etiqueta del archivo.
 * @return bool Verdadero (true) si el desbloqueo fue exitoso o el archivo nunca se abrió
 * @return bool Falso (false) si el archivo AÚN se encuentra bloqueado.
 */
bool correct_unlock(const char *name, const char *tag);

/**
 * Intenta bloquear y desbloquear un mutex para determinar si está libre.
 * 
 * @param mutex Puntero al mutex de pthread a verificar.
 * @return int 0 si el mutex estaba libre y pudo ser bloqueado y liberado exitosamente.
 * @return int -1 si el mutex está actualmente bloqueado por otro hilo.
 */
int mutex_is_free(pthread_mutex_t *mutex);

/**
 * Simula el estado final de un paquete después de haber sido serializado
 * y recibido por la red. Ajusta el 'size' del buffer para que sea igual al 'offset' (datos usados).
 * 
 * @param package El paquete a modificar.
 */
void package_simulate_reception(t_package *package);

#endif

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <commons/log.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>

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
t_log* create_test_logger(void);

/**
 * Destruye un logger de prueba
 * 
 * @param logger Logger a destruir
 */
void destroy_test_logger(t_log* logger);

/**
 * Verifica si un archivo existe
 * 
 * @param file_path Path del archivo a verificar
 * @return 1 si el archivo existe, 0 si no existe
 */
int file_exists(const char* file_path);

/**
 * Verifica si una carpeta existe
 * 
 * @param dir_path Path del carpeta a verificar
 * @return 1 si el carpeta existe, 0 si no existe
 */
int directory_exists(const char* dir_path);

/**
 * Lee el contenido de un archivo en un buffer
 * 
 * @param file_path Path del archivo a leer
 * @param buffer Buffer para almacenar el contenido
 * @param max_size Tamaño maximo a leer
 * @return Numero de bytes leidos, -1 si hay error
 */
int read_file_contents(const char* file_path, char* buffer, size_t max_size);

/**
 * Verifica si una carpeta esta vacia
 * 
 * @param dir_path Path del carpeta a verificar
 * @return 1 si esta vacio, 0 si no esta vacio, -1 si hay error
 */
int is_directory_empty(const char* dir_path);

/**
 * Cuenta el numero de archivos en una carpeta
 * 
 * @param dir_path Path del carpeta
 * @return Numero de archivos, -1 si hay error
 */
int count_files_in_directory(const char* dir_path);

/**
 * Verifica que un archivo tiene el tamaño correcto
 * 
 * @param file_path Path del archivo a verificar
 * @param expected_size Tamaño esperado en bytes
 * @return 1 si el tamaño coincide, 0 si no coincide, -1 si hay error
 */
int verify_file_size(const char* file_path, size_t expected_size);

/**
 * Verifica si dos archivos estan hardlinkeados
 * 
 * @param file1_path Path del primer archivo
 * @param file2_path Path del segundo archivo
 * @return 1 si estan unidos por hard link, 0 si no, -1 si hay error
 */
int files_are_hardlinked(const char* file1_path, const char* file2_path);

/**
 * Crea un archivo superblock.config de prueba con valores predeterminados
 * 
 * @param mount_point Path donde crear el superblock
 * @return 0 en caso de exito, -1 si falla
 */
int create_test_superblock(const char* mount_point);

#endif
#ifndef STORAGE_FS_H
#define STORAGE_FS_H

#include <commons/bitarray.h>
#include <commons/config.h>
#include <commons/log.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "globals/globals.h"

/**
 * Borra todo el contenido del directorio de montaje excepto superblock.config
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @return 0 en caso de exito, -1 si se rompe
 */
int wipe_storage_content(const char* mount_point);


/**
 * Crea el bitmap que dice qué bloques están libres o ocupados
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param fs_size Tamaño total del filesystem
 * @param block_size Tamaño de cada bloque
 * @return 0 en caso de exito, -1 si no puede abrir el archivo, -2 si no hay memoria
 */
int init_bitmap(const char* mount_point, int fs_size, int block_size);

/**
 * Arma el archivo de hashes de bloques para no tener bloques duplicados
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @return 0 en caso de exito, -1 si se rompe
 */
int init_blocks_index(const char* mount_point);

/**
 * Crea el directorio de bloques físicos y todos los archivos de bloques
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param fs_size Tamaño total del filesystem
 * @param block_size Tamaño de cada bloque
 * @return 0 en caso de exito, -1 si no puede crear el directorio, -2 si fallan los bloques, -3 si no hay memoria
 */
int init_physical_blocks(const char* mount_point, int fs_size, int block_size);

/**
 * Arma toda la estructura de archivos con el archivo inicial y sus metadatos
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @return 0 en caso de exito, números negativos (-1 a -6) si se rompe algo
 */
int init_files(const char* mount_point);

/**
 * Monta el filesystem completo ejecutando todas las funciones de inicialización
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @return 0 en caso de exito, números negativos (-1 a -6) que te dicen qué función se rompió
 */
int init_storage(const char* mount_point);

#endif
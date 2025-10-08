#ifndef STORAGE_UTILS_H
#define STORAGE_UTILS_H

#include <commons/config.h>
#include <commons/log.h>
#include <stddef.h>

/**
 * Lee el archivo superblock.config y obtiene la configuración del filesystem
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param fs_size Puntero donde se almacenará el tamaño del filesystem
 * @param block_size Puntero donde se almacenará el tamaño de los bloques
 * @return 0 en caso de éxito, -1 si no puede abrir el archivo, -2 si faltan propiedades
 */
int read_superblock(const char* mount_point, int* fs_size, int* block_size);

/**
 * Calcula el tamaño en bytes necesario para el bitmap del filesystem
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @return Tamaño en bytes del bitmap, -1 si hay error
 */
size_t get_bitmap_size_bytes(const char* mount_point);

/**
 * Modifica múltiples bits en el bitmap del filesystem
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param indexes Array de índices de bloques a modificar
 * @param count Cantidad de elementos en el array indexes
 * @param set_bits 1 para setear bits (marcar como ocupados), 0 para unsetear (marcar como libres)
 * @return 0 en caso de éxito, -1 si hay error abriendo bitmap, -2 si hay error de memoria, -3 si hay error escribiendo
 */
int modify_bitmap_bits(const char* mount_point, int* indexes, size_t count, int set_bits);

#endif
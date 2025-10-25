#ifndef STORAGE_FILE_LOCKS_H_
#define STORAGE_FILE_LOCKS_H_

#include <pthread.h>

/**
 * Estructura que representa un mutex asociado a un archivo específico
 */
typedef struct {
  pthread_mutex_t mutex;
  int ref_count;
} t_file_mutex;

/**
 * Consigue el lock para un file:tag e incrementa incrementa el contador de
 * referencias. Crea el mutex si no existe todavía.
 *
 * @param name Nombre del archivo
 * @param tag Tag del archivo
 */
void lock_file(const char *name, const char *tag);

/**
 * Libera el lock para un file:tag, decrementando el contador de referencias.
 * Si el contador llega a 0, destruye el mutex.
 *
 * @param name Nombre del archivo
 * @param tag Tag del archivo
 */
void unlock_file(const char *name, const char *tag);

/**
 * Limpia todos los file mutexes al terminar el programa
 */
void cleanup_file_sync(void);

#endif

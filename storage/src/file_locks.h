#ifndef STORAGE_FILE_LOCKS_H_
#define STORAGE_FILE_LOCKS_H_

#include <pthread.h>
#include <stdbool.h>

/**
 * Estructura que representa un rwlock asociado a un File:Tag específico
 *
 * El rwlock permite múltiples lectores concurrentes pero escritura exclusiva,
 * optimizando el acceso concurrente al filesystem.
 */
typedef struct {
  pthread_rwlock_t mutex;
  int ref_count;
} t_file_mutex;

/**
 * Consigue el lock para un File:Tag e incrementa el contador de referencias.
 * Crea el rwlock si no existe todavía.
 *
 * @param name Nombre del File
 * @param tag Tag del File
 * @param for_write true para write lock, false para read lock
 */
void lock_file(const char *name, const char *tag, bool for_write);

/**
 * Libera el lock para un File:Tag, decrementando el contador de referencias.
 * Si el contador llega a 0, destruye el rwlock y libera memoria.
 *
 * @param name Nombre del File
 * @param tag Tag del File
 */
void unlock_file(const char *name, const char *tag);

/**
 * Limpia todos los file mutexes al terminar el programa.
 * Destruye todos los rwlocks y libera toda la memoria asociada.
 */
void cleanup_file_sync(void);

#endif

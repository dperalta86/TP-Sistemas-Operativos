#ifndef FILESYSTEM_UTILS_H
#define FILESYSTEM_UTILS_H

#include <commons/config.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEFAULT_DIR_PERMISSIONS 0755
#define FILES_DIR "files"
#define LOGICAL_BLOCKS_DIR "logical_blocks"
#define PHYSICAL_BLOCKS_DIR "physical_blocks"
#define METADATA_CONFIG_FILE "metadata.config"

/**
 * Crea una carpeta recursivamente con mkdir -p
 *
 * @param path Path completo de la carpeta a crear
 * @return 0 en caso de éxito, -1 si falla la creación
 */
int create_dir_recursive(const char *path);

/**
 * Crea toda la estructura de carpetas para un archivo con tag
 * Crea: mount_point/files/file_name/tag/logical_blocks/
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param file_name Nombre del archivo
 * @param tag Tag del archivo
 * @return 0 en caso de éxito, números negativos si falla algún paso
 */
int create_file_dir_structure(const char *mount_point, const char *file_name,
                              const char *tag);

/**
 * Elimina toda la estructura de carpetas para un archivo con tag
 * Elimina recursivamente: mount_point/files/file_name/tag/
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param file_name Nombre del archivo
 * @param tag Tag del archivo a eliminar
 * @return 0 en caso de éxito, -1 si falla
 */
int delete_file_dir_structure(const char *mount_point, const char *file_name,
                              const char *tag);

/**
 * Crea un archivo metadata.config con contenido inicial
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param file_name Nombre del archivo
 * @param tag Tag del archivo
 * @param initial_content Contenido inicial del metadata (puede ser NULL para
 * contenido por defecto)
 * @return 0 en caso de éxito, -1 si no se puede crear el archivo
 */
int create_metadata_file(const char *mount_point, const char *file_name,
                         const char *tag, const char *initial_content);

/**
 * Lee el archivo superblock.config y obtiene la configuración del filesystem
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param fs_size Pointer donde se almacenará el tamaño del filesystem
 * @param block_size Pointer donde se almacenará el tamaño de los bloques
 * @return 0 en caso de éxito, -1 si no puede abrir el archivo, -2 si faltan
 * propiedades
 */
int read_superblock(const char *mount_point, int *fs_size, int *block_size);

/**
 * Modifica bits contiguos en el bitmap del filesystem
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param start_index Índice del primer bloque a modificar
 * @param count Cantidad de bits contiguos a modificar desde start_index
 * @param set_bits 1 para setear bits (marcar como ocupados), 0 para unsetear
 * (marcar como libres)
 * @return 0 en caso de éxito, -1 si hay error abriendo bitmap, -2 si hay error
 * de memoria, -3 si hay error escribiendo, -4 si g_storage_config es NULL
 */
int modify_bitmap_bits(const char *mount_point, int start_index, size_t count,
                       int set_bits);

/**
 * Contiene todos los datos parseados de un archivo metadata.config
 */
typedef struct {
  int size;         // Tamaño del file en bytes
  int *blocks;      // Indexes de bloques físicos asignados
  int block_count;  // Cantidad de bloques en el array
  char *state;      // "WORK_IN_PROGRESS" o "COMMITTED"
  t_config *config; // Config interno (para modificar/guardar después)
} t_file_metadata;

/**
 * Lee el metadata.config de un file:tag
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param filename Nombre del archivo
 * @param tag Tag del archivo
 * @return Pointer a t_file_metadata, o NULL si hay error
 *         NOTE: Destruir con destroy_file_metadata()
 */
t_file_metadata *read_file_metadata(const char *mount_point,
                                    const char *filename, const char *tag);

/**
 * Guarda las modificaciones al struct al disco
 *
 * @param metadata Metadata a guardar
 * @return 0 en caso de éxito, -1 si falla
 */
int save_file_metadata(t_file_metadata *metadata);

/**
 * Destruye el struct
 *
 * @param metadata Struct a liberar (puede ser NULL)
 */
void destroy_file_metadata(t_file_metadata *metadata);

/**
 * Elimina un bloque lógico y libera el bloque físico asociado si ya no es
 * referenciado
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param name Nombre del archivo
 * @param tag Tag del archivo
 * @param logical_block_index Índice del bloque lógico a eliminar
 * @param physical_block_index Índice del bloque físico asociado
 * @param query_id ID de la query para logging
 * @return 0 en caso de éxito, valores negativos en caso de error
 *         -1: Error al eliminar el bloque lógico
 *         -2: Error al obtener estado del bloque físico
 *         -3: Error al liberar el bloque en el bitmap
 */
int delete_logical_block(const char *mount_point, const char *name,
                         const char *tag, int logical_block_index,
                         int physical_block_index, uint32_t query_id);

#endif

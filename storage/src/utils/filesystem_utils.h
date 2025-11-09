#ifndef FILESYSTEM_UTILS_H
#define FILESYSTEM_UTILS_H

#include <commons/config.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <connection/protocol.h>
#include <connection/serialization.h>
#include <commons/config.h>
#include <commons/bitarray.h>
#include "utils/utils.h"

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

/**
 * Verifica la existencia del directorio lógico para un File:Tag.
 * 
 * @param file_name Nombre del archivo.
 * @param tag Tag del archivo.
 * @return bool true si el directorio existe, false en caso contrario.
 */
bool file_dir_exists(const char *file_name, const char *tag);

/**
 * Verifica la existencia del hardlink del bloque lógico de un File:Tag.
 * 
 * @param file_name Nombre del archivo.
 * @param tag Tag del archivo.
 * @param block_number Número de bloque lógico.
 * @param logical_block_path Puntero al buffer donde se guardará la ruta completa.
 * @param path_size Tamaño del buffer logical_block_path.
 * @return bool true si el hardlink existe, false en caso contrario.
 */
bool logical_block_exists(const char *file_name, const char *tag, uint32_t block_number, char *logical_block_path, size_t path_size);

/**
 * Verifica la existencia de un archivo de bloque físico.
 * 
 * @param block_number Número de bloque físico.
 * @param physical_block_path Puntero al buffer donde se guardará la ruta completa.
 * @param path_size Tamaño del buffer physical_block_path.
 * @return bool true si el archivo físico existe, false en caso contrario.
 */
bool physical_block_exists(uint32_t block_number, char *physical_block_path, size_t path_size);

/**
 * Devuelve la cantidad de hardlinks de un bloque físico.
 * 
 * @param logical_block_path Ruta completa a un hardlink lógico que apunta al bloque físico.
 * @return int positivo que indica la cantidad de hardlinks del bloque físico, -1 en caso contrario.
 */
int ph_block_links(char *logical_block_path);

/**
 * Abre el archivo binario del bitmap en el modo especificado.
 * 
 * @param modes Modo de apertura del archivo (ej: "rb", "r+b", "wb").
 * @return FILE* Puntero al archivo abierto, o NULL si falla.
 */
FILE *open_bitmap_file(const char *modes);

/**
 * Carga el bitmap desde disco a un buffer.
 * La función maneja internamente el bloqueo del mutex global g_storage_bitmap_mutex, 
 * pero lo deja desbloqueado en caso de error.
 * 
 * @param bitmap Doble puntero a t_bitarray donde se almacenará la estructura.
 * @param bitmap_buffer Doble puntero al char donde se almacenará el buffer crudo.
 * @return int 0 si la carga es exitosa, un valor negativo en caso de error.
 */
int bitmap_load(t_bitarray **bitmap, char **bitmap_buffer);

/**
 * Persiste el bitmap modificado a disco.
 * 
 * @param bitmap La estructura t_bitarray a persistir (será destruida).
 * @param bitmap_buffer El buffer crudo del bitmap (será liberado).
 * @return int 0 si la persistencia es exitosa, un valor negativo en caso de error.
 */
int bitmap_persist(t_bitarray *bitmap, char *bitmap_buffer);

/**
 * Busca el índice del primer bit libre (0) en el bitmap.
 * 
 * @param bitmap La estructura t_bitarray a inspeccionar.
 * @return ssize_t El índice del bit libre encontrado, o -1 si el bitmap está lleno.
 */
ssize_t get_free_bit_index(t_bitarray *bitmap);

/**
 * Modifica un rango contiguo de bits en el bitmap.
 * 
 * @param bitmap La estructura t_bitarray a modificar.
 * @param start_index Índice del primer bit a modificar.
 * @param count Cantidad de bits a modificar.
 * @param set_bits 1 para setear (a 1) los bits, 0 para limpiar (a 0) los bits.
 */
void set_bitmap_bits(t_bitarray *bitmap, int start_index, size_t count, int set_bits);

/**
 * Crea un paquete de respuesta estándar para notificar un error de protocolo al Worker.
 * * El paquete contiene la Query ID, el código de operación (ej. WRITE_BLOCK_RES)
 * y el código de error específico (ej. FILE_TAG_MISSING).
 * 
 * @param query_id ID de la Query asociada.
 * @param op_code Código de operación del tipo de respuesta (ej. STORAGE_OP_BLOCK_WRITE_RES).
 * @param op_error_code Código de error específico del Storage.
 * @return t_package* El paquete de respuesta de error creado, o NULL si falla la creación del paquete.
 */
t_package *prepare_error_response(uint32_t query_id, t_storage_op_code op_code, int op_error_code);

#endif

#include "filesystem_utils.h"
#include "../errors.h"
#include "../globals/globals.h"
#include <commons/bitarray.h>
#include <commons/config.h>
#include <commons/string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utils/logger.h>
#include <utils/utils.h>

int create_dir_recursive(const char *path) {
  char command[PATH_MAX + 20];
  snprintf(command, sizeof(command), "mkdir -p \"%s\"", path);

  if (system(command) != 0) {
    log_error(g_storage_logger, "No se pudo crear la carpeta %s", path);
    return -1;
  }
  return 0;
}

int create_file_dir_structure(const char *mount_point, const char *file_name,
                              const char *tag) {
  char target_path[PATH_MAX];
  struct stat st;

  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s", mount_point,
           file_name, tag);
  if (stat(target_path, &st) == 0) {
    log_error(g_storage_logger,
              "El tag %s ya existe para el archivo %s, reportando "
              "FILE_TAG_ALREADY_EXISTS",
              tag, file_name);
    return FILE_TAG_ALREADY_EXISTS;
  }

  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s/logical_blocks",
           mount_point, file_name, tag);

  return create_dir_recursive(target_path);
}

int delete_file_dir_structure(const char *mount_point, const char *file_name,
                              const char *tag) {
  char target_path[PATH_MAX];
  struct stat st;
  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s", mount_point,
           file_name, tag);

  if (stat(target_path, &st) != 0) {
    log_warning(g_storage_logger,
                "La carpeta %s no existe, reportando FILE_TAG_MISSING",
                target_path);
    return FILE_TAG_MISSING;
  }

  char command[PATH_MAX + 20];
  snprintf(command, sizeof(command), "rm -rf \"%s\"", target_path);
  if (system(command) != 0) {
    log_error(g_storage_logger, "No se pudo eliminar la carpeta %s",
              target_path);
    return -1;
  }

  return 0;
}

int create_metadata_file(const char *mount_point, const char *file_name,
                         const char *tag, const char *initial_content) {
  char metadata_path[PATH_MAX];

  snprintf(metadata_path, sizeof(metadata_path),
           "%s/files/%s/%s/metadata.config", mount_point, file_name, tag);

  FILE *metadata_ptr = fopen(metadata_path, "w");
  if (metadata_ptr == NULL) {
    log_error(g_storage_logger, "No se pudo crear el archivo %s",
              metadata_path);
    return -1;
  }

  const char *content = initial_content
                            ? initial_content
                            : "SIZE=0\nBLOCKS=[]\nESTADO=WORK_IN_PROGRESS\n";
  fprintf(metadata_ptr, "%s", content);
  fclose(metadata_ptr);

  return 0;
}

int read_superblock(const char *mount_point, int *fs_size, int *block_size) {
  char superblock_path[PATH_MAX];
  snprintf(superblock_path, sizeof(superblock_path), "%s/superblock.config",
           mount_point);

  t_config *config = config_create(superblock_path);
  if (config == NULL) {
    log_error(g_storage_logger, "No se pudo abrir el archivo %s",
              superblock_path);
    return -1;
  }

  if (!config_has_property(config, "FS_SIZE") ||
      !config_has_property(config, "BLOCK_SIZE")) {
    log_error(g_storage_logger, "El superblock no tiene las propiedades "
                                "requeridas (FS_SIZE, BLOCK_SIZE)");
    config_destroy(config);
    return -2;
  }

  *fs_size = config_get_int_value(config, "FS_SIZE");
  *block_size = config_get_int_value(config, "BLOCK_SIZE");

  config_destroy(config);

  log_info(g_storage_logger, "Superblock leído: FS_SIZE=%d, BLOCK_SIZE=%d",
           *fs_size, *block_size);
  return 0;
}

int modify_bitmap_bits(const char *mount_point, int start_index, size_t count,
                       int set_bits) {
  int retval = 0;
  FILE *bitmap_file = NULL;
  char *bitmap_buffer = NULL;
  t_bitarray *bitmap = NULL;

  if (!g_storage_config) {
    log_error(g_storage_logger, "g_storage_config es NULL");
    return -4;
  }

  size_t bitmap_size_bytes = g_storage_config->bitmap_size_bytes;

  char bitmap_path[PATH_MAX];
  snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin", mount_point);

  bitmap_file = fopen(bitmap_path, "r+b");
  if (!bitmap_file) {
    log_error(g_storage_logger, "No se pudo abrir el archivo bitmap: %s",
              bitmap_path);
    retval = -1;
    goto end;
  }

  bitmap_buffer = calloc(1, bitmap_size_bytes);
  if (!bitmap_buffer) {
    log_error(g_storage_logger, "No se pudo asignar memoria para el bitmap");
    retval = -2;
    goto clean_bitmap;
  }

  pthread_mutex_lock(&g_storage_bitmap_mutex);

  if (fread(bitmap_buffer, 1, bitmap_size_bytes, bitmap_file) !=
      bitmap_size_bytes) {
    log_error(g_storage_logger, "No se pudo leer el bitmap completo");
    retval = -1;
    goto unlock_mutex;
  }

  bitmap =
      bitarray_create_with_mode(bitmap_buffer, bitmap_size_bytes, MSB_FIRST);
  if (!bitmap) {
    log_error(g_storage_logger, "No se pudo crear el bitmap en memoria");
    retval = -2;
    goto unlock_mutex;
  }

  for (size_t i = 0; i < count; i++) {
    if (set_bits) {
      bitarray_set_bit(bitmap, start_index + i);
    } else {
      bitarray_clean_bit(bitmap, start_index + i);
    }
  }

  fseek(bitmap_file, 0, SEEK_SET);

  int written_bytes = fwrite(bitmap_buffer, 1, bitmap_size_bytes, bitmap_file);

  if (written_bytes != bitmap_size_bytes) {
    log_error(g_storage_logger, "No se pudo escribir el bitmap modificado");
    retval = -3;
  } else {
    log_info(g_storage_logger, "Modificados %zu bits en el bitmap (%s)", count,
             set_bits ? "seteados" : "unseteados");
  }

  bitarray_destroy(bitmap);
unlock_mutex:
  pthread_mutex_unlock(&g_storage_bitmap_mutex);
clean_bitmap:
  free(bitmap_buffer);
  fclose(bitmap_file);
end:
  return retval;
}

t_file_metadata *read_file_metadata(const char *mount_point,
                                    const char *filename, const char *tag) {
  char metadata_path[PATH_MAX];
  snprintf(metadata_path, sizeof(metadata_path),
           "%s/files/%s/%s/metadata.config", mount_point, filename, tag);

  t_config *config = config_create(metadata_path);
  if (!config) {
    log_error(g_storage_logger, "No se pudo abrir el metadata.config: %s",
              metadata_path);
    return NULL;
  }

  if (!config_has_property(config, "SIZE") ||
      !config_has_property(config, "BLOCKS") ||
      !config_has_property(config, "ESTADO")) {
    log_error(g_storage_logger,
              "El metadata.config no tiene las propiedades requeridas "
              "(SIZE, BLOCKS, ESTADO)");
    config_destroy(config);
    return NULL;
  }

  t_file_metadata *metadata = malloc(sizeof(t_file_metadata));
  if (!metadata) {
    log_error(g_storage_logger,
              "No se pudo asignar memoria para t_file_metadata");
    config_destroy(config);
    return NULL;
  }

  metadata->size = config_get_int_value(config, "SIZE");

  char *state_value = config_get_string_value(config, "ESTADO");
  metadata->state = string_duplicate(state_value);

  char **blocks_str = config_get_array_value(config, "BLOCKS");
  metadata->block_count = string_array_size(blocks_str);

  if (metadata->block_count > 0) {
    metadata->blocks = malloc(sizeof(int) * metadata->block_count);
    if (!metadata->blocks) {
      log_error(g_storage_logger,
                "No se pudo asignar memoria para el array de bloques");
      string_array_destroy(blocks_str);
      free(metadata->state);
      free(metadata);
      config_destroy(config);
      return NULL;
    }

    for (int i = 0; i < metadata->block_count; i++) {
      metadata->blocks[i] = atoi(blocks_str[i]);
    }
  } else {
    metadata->blocks = NULL;
  }

  metadata->config = config;
  string_array_destroy(blocks_str);

  log_info(g_storage_logger,
           "Metadata leído: %s:%s - SIZE=%d, BLOCKS=%d, ESTADO=%s", filename,
           tag, metadata->size, metadata->block_count, metadata->state);

  return metadata;
}

int save_file_metadata(t_file_metadata *metadata) {
  if (!metadata || !metadata->config) {
    log_error(g_storage_logger, "Metadata o config es NULL");
    return -1;
  }

  char field_str[32];
  snprintf(field_str, sizeof(field_str), "%d", metadata->size);
  config_set_value(metadata->config, "SIZE", field_str);

  // Si no tenemos BLOCKS, escribimos `[]`
  char *stringified_blocks;
  if (metadata->blocks == NULL || metadata->block_count == 0) {
    stringified_blocks = string_duplicate("[]");
  } else {
    char **blocks_str_array = string_array_new();

    for (int i = 0; i < metadata->block_count; i++) {
      snprintf(field_str, sizeof(field_str), "%d", metadata->blocks[i]);
      string_array_push(&blocks_str_array, string_duplicate(field_str));
    }

    stringified_blocks = get_stringified_array(blocks_str_array);
    string_array_destroy(blocks_str_array);
  }

  config_set_value(metadata->config, "BLOCKS", stringified_blocks);
  free(stringified_blocks);

  config_set_value(metadata->config, "ESTADO", metadata->state);

  config_save(metadata->config);
  log_info(g_storage_logger, "Metadata guardada");
  return 0;
}

void destroy_file_metadata(t_file_metadata *metadata) {
  if (!metadata) {
    return;
  }

  if (metadata->blocks)
    free(metadata->blocks);

  if (metadata->state)
    free(metadata->state);

  if (metadata->config)
    config_destroy(metadata->config);

  free(metadata);
}

int delete_logical_block(const char *mount_point, const char *name,
                         const char *tag, int logical_block_index,
                         int physical_block_index, uint32_t query_id) {
  char target_path[PATH_MAX];
  snprintf(target_path, sizeof(target_path),
           "%s/files/%s/%s/logical_blocks/%04d.dat", mount_point, name, tag,
           logical_block_index);

  if (remove(target_path) != 0) {
    log_error(g_storage_logger,
              "No se pudo eliminar el bloque lógico %04d en %s",
              logical_block_index, target_path);
    return -1;
  }

  log_info(g_storage_logger,
           "##%u - Bloque Lógico Eliminado - Nombre: %s, Tag: %s, "
           "Índice: %04d",
           query_id, name, tag, logical_block_index);

  snprintf(target_path, sizeof(target_path), "%s/physical_blocks/block%04d.dat",
           mount_point, physical_block_index);

  struct stat statbuf;
  if (stat(target_path, &statbuf) != 0) {
    log_error(g_storage_logger,
              "No se pudo obtener el estado del bloque físico %04d en %s",
              physical_block_index, target_path);
    return -2;
  }

  if (statbuf.st_nlink != 1) {
    log_info(g_storage_logger,
             "El bloque físico %04d todavía tiene %lu hard links, no se libera",
             physical_block_index, statbuf.st_nlink);
    return 0;
  }

  if (modify_bitmap_bits(mount_point, physical_block_index, 1, 0) != 0) {
    log_error(g_storage_logger,
              "No se pudo liberar el bloque físico %04d en el bitmap",
              physical_block_index);
    return -3;
  }

  log_info(g_storage_logger,
           "##%u - Bloque Físico Liberado - Número de Bloque: %04d", query_id,
           physical_block_index);

  return 0;
}

bool file_dir_exists(const char *file_name, const char *tag) {
  char tag_path[PATH_MAX];
  struct stat st;

  snprintf(tag_path, sizeof(tag_path), "%s/files/%s/%s",
           g_storage_config->mount_point, file_name, tag);

  return stat(tag_path, &st) == 0 && S_ISDIR(st.st_mode);
}

t_package *prepare_error_response(uint32_t query_id, t_storage_op_code op_code,
                                  int op_error_code) {
  t_package *response = package_create_empty(op_code);
  if (!response) {
    log_error(g_storage_logger,
              "## Query ID: %d - Fallo al crear paquete de respuesta de error",
              query_id);
    return NULL;
  }

  if (!package_add_int8(response, op_error_code)) {
    log_error(g_storage_logger,
              "## Query ID: %d - Fallo al agregar el código de error al "
              "paquete de respuesta",
              query_id);
    package_destroy(response);
    return NULL;
  }

  return response;
}

bool logical_block_exists(const char *file_name, const char *tag,
                          uint32_t block_number, char *logical_block_path,
                          size_t path_size) {
  snprintf(logical_block_path, path_size,
           "%s/files/%s/%s/logical_blocks/%04d.dat",
           g_storage_config->mount_point, file_name, tag, block_number);

  return regular_file_exists(logical_block_path);
}

bool physical_block_exists(uint32_t block_number, char *physical_block_path,
                           size_t path_size) {
  snprintf(physical_block_path, path_size, "%s/physical_blocks/block%04d.dat",
           g_storage_config->mount_point, block_number);

  return regular_file_exists(physical_block_path);
}

int ph_block_links(char *logical_block_path) {
  struct stat file_stat;
  if (stat(logical_block_path, &file_stat) != 0) {
    log_error(g_storage_logger, "No se pudo obtener el estado del bloque %s",
              logical_block_path);
    return -1;
  }

  return file_stat.st_nlink;
}

ssize_t get_free_bit_index(t_bitarray *bitmap) {
  size_t max_bit = bitarray_get_max_bit(bitmap);

  for (size_t i = 0; i < max_bit; i++) {
    if (!bitarray_test_bit(bitmap, i)) {
      return (ssize_t)i;
    }
  }
  return -1;
}

FILE *open_bitmap_file(const char *modes) {
  char bitmap_path[PATH_MAX];
  snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin",
           g_storage_config->mount_point);

  FILE *bitmap_file = fopen(bitmap_path, modes);
  if (!bitmap_file) {
    log_error(g_storage_logger, "No se pudo abrir el archivo bitmap: %s",
              bitmap_path);
    return NULL;
  }

  return bitmap_file;
}

int bitmap_load(t_bitarray **bitmap, char **bitmap_buffer) {
  int retval = 0;
  size_t bitmap_size_bytes = g_storage_config->bitmap_size_bytes;

  FILE *bitmap_file = open_bitmap_file("rb");
  if (bitmap_file == NULL) {
    retval = -1;
    goto end;
  }

  *bitmap_buffer = calloc(1, bitmap_size_bytes);
  if (!*bitmap_buffer) {
    log_error(g_storage_logger, "No se pudo asignar memoria para el bitmap");
    retval = -2;
    goto clean_file;
  }

  pthread_mutex_lock(&g_storage_bitmap_mutex);

  if (fread(*bitmap_buffer, 1, bitmap_size_bytes, bitmap_file) !=
      bitmap_size_bytes) {
    log_error(g_storage_logger, "No se pudo leer el bitmap completo");
    pthread_mutex_unlock(&g_storage_bitmap_mutex);
    free(*bitmap_buffer);
    retval = -3;
    goto clean_file;
  }

  *bitmap =
      bitarray_create_with_mode(*bitmap_buffer, bitmap_size_bytes, MSB_FIRST);
  if (!*bitmap) {
    log_error(g_storage_logger, "No se pudo crear el bitmap en memoria");
    pthread_mutex_unlock(&g_storage_bitmap_mutex);
    free(*bitmap_buffer);
    retval = -4;
  }

clean_file:
  if (bitmap_file)
    fclose(bitmap_file);
end:
  return retval;
}

int bitmap_persist(t_bitarray *bitmap, char *bitmap_buffer) {
  int retval = 0;
  size_t bitmap_size_bytes = g_storage_config->bitmap_size_bytes;

  FILE *bitmap_file = open_bitmap_file("r+b");
  if (bitmap_file == NULL) {
    retval = -1;
    goto end;
  }

  fseek(bitmap_file, 0, SEEK_SET);

  int written_bytes = fwrite(bitmap_buffer, 1, bitmap_size_bytes, bitmap_file);

  if (written_bytes != (int)bitmap_size_bytes) {
    log_error(g_storage_logger, "No se pudo escribir el bitmap modificado");
    retval = -2;
    goto clean_file;
  }

  log_info(g_storage_logger, "Bitmap persistido correctamente");

clean_file:
  if (bitmap_file)
    fclose(bitmap_file);
end:
  if (bitmap)
    bitarray_destroy(bitmap);
  if (bitmap_buffer)
    free(bitmap_buffer);
  pthread_mutex_unlock(&g_storage_bitmap_mutex);
  return retval;
}

void set_bitmap_bits(t_bitarray *bitmap, int start_index, size_t count,
                     int set_bits) {
  for (size_t i = 0; i < count; i++) {
    if (set_bits) {
      bitarray_set_bit(bitmap, start_index + i);
    } else {
      bitarray_clean_bit(bitmap, start_index + i);
    }
  }

  log_info(g_storage_logger, "Modificados %zu bits en el bitmap buffer (%s)",
           count, set_bits ? "seteados" : "unseteados");
}

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
    goto clean_file;
  }

  pthread_mutex_lock(&g_storage_bitmap_mutex);

  if (fread(bitmap_buffer, 1, bitmap_size_bytes, bitmap_file) !=
      bitmap_size_bytes) {
    log_error(g_storage_logger, "No se pudo leer el bitmap completo");
    retval = -1;
    goto clean_buffer;
  }

  bitmap =
      bitarray_create_with_mode(bitmap_buffer, bitmap_size_bytes, MSB_FIRST);
  if (!bitmap) {
    log_error(g_storage_logger, "No se pudo crear el bitmap en memoria");
    retval = -2;
    goto clean_buffer;
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

  pthread_mutex_unlock(&g_storage_bitmap_mutex);

  if (written_bytes != bitmap_size_bytes) {
    log_error(g_storage_logger, "No se pudo escribir el bitmap modificado");
    retval = -3;
    goto clean_bitmap;
  }

  log_info(g_storage_logger, "Modificados %zu bits en el bitmap (%s)", count,
           set_bits ? "seteados" : "unseteados");

clean_bitmap:
  if (bitmap)
    bitarray_destroy(bitmap);
clean_buffer:
  if (bitmap_buffer)
    free(bitmap_buffer);
clean_file:
  if (bitmap_file)
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

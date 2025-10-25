#include "file_locks.h"
#include "fresh_start/fresh_start.h"
#include "globals/globals.h"
#include "server/server.h"
#include <commons/bitarray.h>
#include <commons/config.h>
#include <commons/log.h>
#include <config/storage_config.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils/client_socket.h>
#include <utils/server.h>
#include <utils/utils.h>

#define MODULO "STORAGE"
#define DEFAULT_CONFIG_PATH "./storage.config"

int main(int argc, char *argv[]) {
  // Obtiene posibles parametros de entrada
  char config_file_path[PATH_MAX];
  int retval = 0;

  if (argc == 1) {
    strlcpy(config_file_path, DEFAULT_CONFIG_PATH, PATH_MAX);
  } else if (argc == 2) {
    strlcpy(config_file_path, argv[1], PATH_MAX);
  } else {
    fprintf(stderr, "Solo se puede ingresar el argumento [archivo_config]\n");
    retval = -1;
    goto error;
  }

  // Crea storage config como variable global
  g_storage_config = create_storage_config(config_file_path);
  if (g_storage_config == NULL) {
    fprintf(stderr, "No se pudo cargar la configuración\n");
    retval = -2;
    goto error;
  }

  // Crea storage logger como variable global
  char current_directory[PATH_MAX];
  if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
    fprintf(stderr, "Error al obtener el directorio actual\n");
    retval = -3;
    goto clean_config;
  }

  g_storage_logger = create_logger(current_directory, MODULO, false,
                                   g_storage_config->log_level);
  if (g_storage_logger == NULL) {
    fprintf(stderr, "No se pudo crear el logger\n");
    retval = -4;
    goto clean_config;
  }

  log_debug(g_storage_logger, "Logger creado exitosamente.");
  log_debug(g_storage_logger,
            "Configuracion leida: "
            "\\n\\tSTORAGE_IP=%s\\n\\tSTORAGE_PORT=%s\\n\\tFRESH_START=%"
            "s\\n\\tMOUNT_POINT=%s\\n\\tOPERATION_DELAY=%d\\n\\tBLOCK_ACCESS_"
            "DELAY=%d\\n\\tLOG_LEVEL=%s",
            g_storage_config->storage_ip, g_storage_config->storage_port,
            g_storage_config->fresh_start ? "TRUE" : "FALSE",
            g_storage_config->mount_point, g_storage_config->operation_delay,
            g_storage_config->block_access_delay,
            log_level_as_string(g_storage_config->log_level));

  // Inicializa diccionario de file locks
  g_open_files_dict = dictionary_create();

  // Verifica si se realiza fresh start
  if (g_storage_config->fresh_start) {
    log_info(
        g_storage_logger,
        "Iniciando en modo FRESH_START, se eliminará el contenido previo en %s",
        g_storage_config->mount_point);

    int init_result = init_storage(g_storage_config->mount_point);
    if (init_result != 0) {
      log_error(g_storage_logger, "Error al inicializar el filesystem: %d",
                init_result);
      retval = -6;
      goto clean_logger;
    }

    log_info(g_storage_logger, "Filesystem inicializado exitosamente en %s",
             g_storage_config->mount_point);
  }

  // Inicia servidor
  int socket = start_server(g_storage_config->storage_ip,
                            g_storage_config->storage_port);
  if (socket < 0) {
    log_error(g_storage_logger, "No se pudo iniciar el servidor en %s:%s\n",
              g_storage_config->storage_ip, g_storage_config->storage_port);
    retval = -5;
    goto clean_logger;
  }

  log_info(g_storage_logger, "Servidor iniciado en %s:%s",
           g_storage_config->storage_ip, g_storage_config->storage_port);

  while (1) {
    int client_fd = wait_for_client(socket);
    if (client_fd == -1) {
      goto clean_logger;
    }

    t_client_data *client_data = malloc(sizeof(t_client_data));
    if (client_data == NULL) {
      log_error(g_storage_logger,
                "Error al asignar memoria para los datos del cliente %d. Se "
                "cierra la conexión.",
                client_fd);
      close(client_fd);
      continue;
    }
    client_data->client_socket = client_fd;

    pthread_t client_thread;
    if (pthread_create(&client_thread, NULL, handle_client,
                       (void *)client_data) != 0) {
      log_error(g_storage_logger,
                "Error al crear hilo para cliente %d. Se cierra la conexión.",
                client_fd);
      close(client_fd);
      free(client_data);
      continue;
    }

    pthread_detach(client_thread);
  }

  close(socket);
  cleanup_file_sync();
  log_destroy(g_storage_logger);
  destroy_storage_config(g_storage_config);
  exit(EXIT_SUCCESS);

clean_logger:
  cleanup_file_sync();
  log_destroy(g_storage_logger);
clean_config:
  destroy_storage_config(g_storage_config);

error:
  return retval;
}

#include <utils/server.h>
#include <utils/utils.h>
#include <config/master_config.h>
#include <commons/config.h>
#include <commons/log.h>
#include <linux/limits.h>

#define MODULO "MASTER"
#define LOG_LEVEL "DEBUG"

int main(int argc, char* argv[]) {

    // Verifico que se hayan pasado los parametros correctamente
    if (argc != 2) {
        printf("[ERROR]: Se esperaban ruta al archivo de configuracion\nUso: %s [archivo_config]\n", argv[0]);
        goto error;
    }

    // Obtengo el directorio actual
    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        fprintf(stderr, "Error al obtener el directorio actual\n");
        goto error;
    }
    
    // Inicializo el logger
    t_log* logger = create_logger(current_directory, MODULO, true, LOG_LEVEL);
    if (logger == NULL) {
        fprintf(stderr, "Error al crear el logger\n");
        goto error;
    }

    char* path_config_file = argv[1];

    // Cargo el archivo de configuracion 
    t_master_config *master_config = create_master_config(path_config_file);
    if (!master_config)
    {
        log_error(logger, "Error al leer el archivo de configuracion %s\n", path_config_file);
        goto clean;
    }

    log_debug(logger, "Configuracion leida: \n\tIP_ESCUCHA=%s\n\tPUERTO_ESCUCHA=%s\n\tALGORITMO_PLANIFICACION=%s\n\tTIEMPO_AGING=%d\n\tLOG_LEVEL=%s",
             master_config->ip, master_config->port, master_config->scheduler_algorithm, master_config->aging_time, LOG_LEVEL);

    // Inicio el servidor
    int socket = start_server(master_config->ip, master_config->port);

    if (socket == -1) {
        log_error(logger, "Error al iniciar el servidor en %s:%s", master_config->ip, master_config->port);
        goto clean;
    }

    log_info(logger, "Socket %d creado con exito!", socket);

clean:
    destroy_config(master_config);
    log_destroy(logger);
    return 1;
error:
    return -1;

    return 0;
}

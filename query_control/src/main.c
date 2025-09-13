#include <utils/hello.h>
#include <utils/server.h>

int main(int argc, char* argv[])
{
    int retval = 0;
    char* config_filepath = argv[1];

    saludar("query_control");

    t_log* logger = create_logger(current_directory, MODULO, true, LOG_LEVEL);
    if (logger == NULL)
    {
        fprintf(stderr, "Error al crear el logger\n");
        goto error;
    }

    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        log_error(logger, "Error al obtener el directorio actual\n");
        goto clean_logger;
    }

    t_master_config *master_config = create_master_config(config_filepath);
    if (!master_config)
    {
        log_error(logger, "Error al leer el archivo de configuracion %s\n", config_filepath);
        goto clean;
    }
    log_debug(logger,
              "Configuracion leida: \n\tIP_ESCUCHA=%s\n\tPUERTO_ESCUCHA=%s\n\tLOG_LEVEL=%s",
              query_control_config->ip,
              query_control_config->port,
              query_control_config->log_level);

clean_logger:
    log_destroy(logger);
error:
    return retval;
}

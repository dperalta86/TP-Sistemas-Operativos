#include <commons/config.h>
#include <commons/log.h>
#include <config/query_control_config.h>
#include <linux/limits.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utils/hello.h>
#include <utils/server.h>
#include <utils/utils.h>
#include <connection/protocol.h>
#include <connection/serialization.h>

#define MODULO "QUERY_CONTROL"

// Abstraccion de manejo de errores
static inline int fail_pkg(t_log* logger, const char* msg, t_package** pkgr, int code) {
    if (logger && msg) log_error(logger, "%s", msg);
    if (pkgr && *pkgr) { package_destroy(*pkgr); *pkgr = NULL; }
    return code;
}

int main(int argc, char* argv[])
{
    int retval = 0;

    if (argc != 4) {
        printf("[ERROR]: Se esperaban ruta al archivo de configuracion, archivo de query y prioridad\nUso: %s [archivo_config] [archivo_query] [prioridad]\n", argv[0]);
        retval = -1;
        goto error;
    }

    char* config_filepath = argv[1];
    char* query_filepath = argv[2];
    int priority = atoi(argv[3]);


    if (priority < 0) {
        printf("[ERROR]: La prioridad no puede ser negativa\n");
        retval = -8;
        goto error;
    }

    saludar("query_control");

    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        fprintf(stderr, "Error al obtener el directorio actual\n");
        retval = -2;
        goto error;
    }

    t_query_control_config *query_control_config = create_query_control_config(config_filepath);
    if (!query_control_config)
    {
        fprintf(stderr, "Error al leer el archivo de configuracion %s\n", config_filepath);
        retval = -3;
        goto clean_config;
    }

    t_log* logger = create_logger(current_directory, MODULO, true, query_control_config->log_level);
    if (logger == NULL)
    {
        fprintf(stderr, "Error al crear el logger\n");
        retval = -4;
        goto clean_config;
    }

    log_debug(logger,
              "Configuracion leida: \n\tIP_MASTER=%s\n\tPUERTO_MASTER=%s\n\tLOG_LEVEL=%s",
              query_control_config->ip,
              query_control_config->port,
              log_level_as_string(query_control_config->log_level));

    int master_socket = connect_to_server(query_control_config->ip, query_control_config->port);
    if (master_socket < 0) {
        log_error(logger, "Error al conectar con el master en %s:%s", query_control_config->ip, query_control_config->port);
        retval = -5;
        goto clean_logger;
    }

    // Enviar handshake al master
    // Creo un nuevo package con el op_code
    t_package *package_handshake = package_create_empty(OP_QUERY_HANDSHAKE);
    if (package_handshake == NULL) 
    {
        log_error(logger, "Error al crear el paquete para el handshake al master");
        retval = -6;
        goto clean_socket;
    }

    // agrego un string como campo del buffer y verifico Éxito
    if (!package_add_string(package_handshake, "QUERY_CONTROL_HANDSHAKE")) {
        retval = fail_pkg(logger, "Error al agregar string al paquete handshake", &package_handshake, -6);
        goto clean_socket;
    }

    // Envio del paquete
    if (package_send(package_handshake, master_socket) != 0) {
        retval = fail_pkg(logger, "Error al enviar el paquete handshake al master", &package_handshake, -6);
        goto clean_socket;
    }

    // Destruyo el package
    package_destroy(package_handshake);

    // Preparo para recibir respuesta
    t_package *response_package = package_receive(master_socket);

    if (!response_package) {
        retval = fail_pkg(logger, "Error al recibir respuesta de handshake", &response_package, -6);
        goto clean_socket;
    }
    if (response_package->operation_code != OP_QUERY_HANDSHAKE) {
        retval = fail_pkg(logger, "Handshake inválido (opcode inesperado)", &response_package, -6);
        goto clean_socket;
    }

    package_destroy(response_package);
    
    log_info(logger, "## Conexión al Master exitosa. IP: %s, Puerto: %s.", query_control_config->ip, query_control_config->port);

    // Comienza petición de ejecución de query
    log_info(logger, "## Solicitud de ejecución de Query: %s, prioridad: %d", query_filepath, priority);

    t_package* package_to_send = package_create_empty(OP_QUERY_FILE_PATH);
    if (!package_to_send) {
        log_error(logger, "Error al crear el paquete para envío de Query");
        retval = -6;
        goto clean_socket;
    }

    // Ejecutar y validar ambos package_add
    if (!package_add_string(package_to_send, query_filepath)) {
        retval = fail_pkg(logger, "Error al agregar path de Query al paquete", &package_to_send, -6);
        goto clean_socket;
    }
    if (!package_add_uint8(package_to_send, (uint8_t)priority)) {
        retval = fail_pkg(logger, "Error al agregar prioridad al paquete", &package_to_send, -6);
        goto clean_socket;
    }

    if (package_send(package_to_send, master_socket) < 0) {
        retval = fail_pkg(logger, "Error al enviar paquete con Query al Master", &package_to_send, -6);
        goto clean_socket;  
    }

    package_destroy(package_to_send);


    // Preparo para recibir respuesta
    response_package = package_receive(master_socket); 
    
    if (!response_package || response_package->operation_code != QC_OP_MASTER_CONNECTION_OK)
    {
        retval = fail_pkg(logger, "Error al recibir respuesta de conexión de Master", &response_package, -7); 
    }

    log_info(logger, "Paquete con path de query: %s y prioridad: %d enviado al master correctamente", query_filepath, priority);


    // === Loop de recepción: READ / FIN ===
    while (true) {
    t_package *resp = package_receive(master_socket);

    if (!resp) {
        log_error(logger, "Conexión con Master cerrada inesperadamente");
        retval = -7;
        break;
    }

    switch (resp->operation_code) {

        case QC_OP_READ_DATA: {

            char* file_tag = package_read_string(resp);  
            size_t size = 0; 
            void* file_data = package_read_data(resp, &size);

            if(file_tag == NULL){
                retval = fail_pkg(logger, "El fileTag recibido es nulo", &resp, -7);
                goto clean_socket;

            }
            else if(file_data == NULL){
                 retval = fail_pkg(logger, "", &resp, -7); 
                 log_error(logger, "No se leyo ningun dato del archivo %s", file_tag);  
                 free(file_tag); 
                 goto clean_socket;             
            } 


            char* contenido = malloc(size + 1);

            if (!contenido) {
                retval = fail_pkg(logger, "Memoria insuficiente al procesar READ_DATA", &resp, -7);
                free(file_tag); free(file_data);
                goto clean_socket;
            }
            memcpy(contenido, file_data, size);
            contenido[size] = '\0';

            // Estructura <File:Tag> debe venir de master en un solo string listo para logear
            log_info(logger, "## Lectura realizada: File %s, contenido: %s", file_tag, contenido);

            free(file_tag);
            free(file_data);
            free(contenido);
        } break;

        case QC_OP_MASTER_FIN_DESCONEXION:
        case QC_OP_MASTER_FIN_PRIORIDAD: {


            const char* motivoString =
            (resp->operation_code == QC_OP_MASTER_FIN_DESCONEXION) ? "DESCONEXION" : "PRIORIDAD";

            log_info(logger, "## Query Finalizada - %s", motivoString);

            package_destroy(resp); resp = NULL;
            retval = 0;
            goto clean_socket;
        } break;
        
        default:
            log_warning(logger, "Error al recibir respuesta, opcode %u desconocido", resp->operation_code);
            retval = fail_pkg(logger, "", &resp, -7);
            break;
    }

    package_destroy(resp);
}

    package_destroy(response_package);

clean_socket:
    close(master_socket);
clean_logger:
    log_destroy(logger);
clean_config:
    destroy_query_control_config_instance(query_control_config);
error:
    return retval;



}


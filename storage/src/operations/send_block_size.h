#ifndef STORAGE_OPERATIONS_SEND_BLOCK_SIZE_H
#define STORAGE_OPERATIONS_SEND_BLOCK_SIZE_H

#include "connection/serialization.h"
#include "connection/protocol.h"
#include "globals/globals.h"

/**
 * Maneja la solicitud de tamaño de bloque devolviendo un paquete con dicho dato y su respectivo
 * código de operación.
 *
 * @param client_data  Datos del cliente (Worker) que solicita el tamaño de bloque.
 * @return t_package*  Puntero al paquete con la información del tamaño de bloque, 
 *                     o NULL en caso de error.
 */
t_package* send_block_size(t_client_data *client_data);

#endif
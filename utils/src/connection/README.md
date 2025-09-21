# 📡 Protocolo de Comunicación - TP Sistemas Operativos

Librería robusta para comunicación entre módulos del TP. Maneja automáticamente serialización, endianness, y validaciones de seguridad.

## 🚀 Quick Start

```c
#include "serialization.h"

// Enviar
t_package *pkg = package_create_empty(OP_LOGIN_REQUEST);
package_add_string(pkg, "usuario");
package_add_string(pkg, "password");
package_send(pkg, socket);
package_destroy(pkg);

// Recibir  
t_package *pkg = package_receive(socket);
char *user = package_read_string(pkg);
char *pass = package_read_string(pkg);
package_destroy(pkg);
```

## 📋 Tipos de Datos Soportados

| Función | Descripción | Tamaño en Red |
|---------|-------------|---------------|
| `package_add_uint8(pkg, value)` | Entero 8 bits | 1 byte |
| `package_add_uint16(pkg, value)` | Entero 16 bits | 2 bytes |
| `package_add_uint32(pkg, value)` | Entero 32 bits | 4 bytes |
| `package_add_string(pkg, str)` | String con longitud | 4 bytes + len |
| `package_add_struct(pkg, &struct)` | Struct completo | 4 bytes + sizeof |

## Definición de Protocolo

**1. Definir enum de operaciones (compartido por todo el equipo):**

```c
typedef enum {
    OP_QUERY_HANDSHAKE,
    OP_QUERY_FILE_PATH,
    OP_WORKER_HANDSHAKE_REQ,
    OP_WORKER_HANDSHAKE_RES,
} t_master_op_code;
```

**2. Documentar estructura de cada mensaje:**
Ejemplos:  

```c
// OP_LOGIN_REQUEST:
//   - username: string
//   - password: string

// OP_TRANSACTION_REQUEST:
//   - transaction_data: transaction_t struct  
//   - description: string
//   - timestamp: uint32

// OP_ERROR:
//   - error_code: uint32
//   - error_message: string
```

## Ejemplos Prácticos

### Envío de Login
```c
void send_login(int socket, const char *user, const char *pass) {
    t_package *pkg = package_create_empty(OP_LOGIN_REQUEST);
    
    if (!package_add_string(pkg, user) || 
        !package_add_string(pkg, pass)) {
        printf("Error agregando datos\n");
        package_destroy(pkg);
        return;
    }
    
    if (package_send(pkg, socket) < 0) {
        printf("Error enviando paquete\n");
    }
    
    package_destroy(pkg);
}
```

### Envío de Struct
```c
typedef struct {
    uint32_t id;
    uint32_t amount;
    uint8_t type;
} transaction_t;

void send_transaction(int socket, transaction_t *trans, const char *desc) {
    t_package *pkg = package_create_empty(OP_TRANSACTION_REQUEST);
    
    package_add_struct(pkg, trans);  // ← Macro para structs
    package_add_string(pkg, desc);
    package_add_uint32(pkg, time(NULL));
    
    package_send(pkg, socket);
    package_destroy(pkg);
}
```

### Recepción y Switch
```c
void handle_message(int socket) {
    t_package *pkg = package_receive(socket);
    if (!pkg) {
        printf("Error recibiendo paquete\n");
        return;
    }
    
    switch (pkg->operation_code) {
        case OP_LOGIN_REQUEST:
            handle_login(pkg);
            break;
        case OP_TRANSACTION_REQUEST:
            handle_transaction(pkg);
            break;
        default:
            printf("Op code desconocido: %u\n", pkg->operation_code);
    }
    
    package_destroy(pkg);
}

void handle_login(t_package *pkg) {
    char *user = package_read_string(pkg);
    char *pass = package_read_string(pkg);
    
    // Procesar login...
    bool success = authenticate(user, pass);
    
    // Enviar respuesta
    send_login_response(socket, success);
    
    free(user);
    free(pass);
}
```

## Características Principales

### **Automático y Seguro**
- Buffer crece dinámicamente
- Conversión automática a big-endian (network byte order)
- Validaciones de seguridad (límites de memoria)
- Manejo robusto de conexiones interrumpidas

### **Fácil de Usar** 
- API simple de 3 pasos: crear → agregar → enviar
- Macros para structs: `package_add_struct()`, `package_read_struct()`  
- Sin cálculos manuales de tamaño
- Manejo automático de memoria

### **Robusto**
- Funciones retornan `bool` para validar errores
- Recv garantizado (no se pierden bytes)
- Límites de seguridad configurables
- Compatible entre arquitecturas diferentes

## Mejoras incorporadas:

Mayor facilidad de uso, calculo de tamaño automático (disminuye errores).
Se genera de manera automática el envío en Big-Endian.
Mejora en el manejo de errores y seguridad (limites y validadciones para evitar buffer overflow).

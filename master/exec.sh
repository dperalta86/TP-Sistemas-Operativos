#!/bin/bash

# Verificar que haya al menos un argumento
if [ $# -lt 1 ]; then
    echo "Uso: $0 <nombre_script_de_prueba>"
    exit 1
fi

# Normalizar el nombre a minúsculas
SCRIPT_NAME=$(echo "$1" | tr '[:upper:]' '[:lower:]')

# Armar ruta del script
SCRIPT_PATH="./scripts/${SCRIPT_NAME}.sh"

# Validar existencia del archivo
if [ ! -f "$SCRIPT_PATH" ]; then
    echo "ERROR: No se encontró el script: $SCRIPT_PATH"
    exit 1
fi

chmod +x "$SCRIPT_PATH"

# Ejecutar el script y pasarle los parámetros restantes
"$SCRIPT_PATH"

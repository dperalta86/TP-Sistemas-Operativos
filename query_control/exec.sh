#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Uso: ./exec.sh <script_de_prueba> <query_control_id>"
    exit 1
fi

SCRIPT_NAME=$(echo "$1" | tr '[:upper:]' '[:lower:]')
QUERY_CONTROL_ID="$2"

# Validación del ID del QUERY_CONTROL
if ! [[ "$QUERY_CONTROL_ID" =~ ^[0-9]+$ ]]; then
    echo "ERROR: El query_control_id debe ser un número entero."
    exit 1
fi

SCRIPT_PATH="./scripts/${SCRIPT_NAME}.sh"

# Verificar si el script existe
if [ ! -f "$SCRIPT_PATH" ]; then
    echo "ERROR: El script '$SCRIPT_PATH' no existe."
    exit 1
fi

echo "Ejecutando script: $SCRIPT_PATH con query_control_id=$QUERY_CONTROL_ID"
chmod +x "$SCRIPT_PATH"

# Ejecutar el segundo script pasándole el query_control_id
"$SCRIPT_PATH" "$QUERY_CONTROL_ID"

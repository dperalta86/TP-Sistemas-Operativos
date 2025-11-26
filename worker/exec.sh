#!/bin/bash

# ============================================================================
# Script: exec.sh
# Uso:    ./exec.sh <nombre_script> <worker_id>
# Descripción:
#   - Normaliza el nombre del script a minúsculas
#   - Valida que el worker_id sea numérico
#   - Ejecuta el script correspondiente bajo ./scripts/ pasándole el worker_id
# ============================================================================

if [ $# -ne 2 ]; then
    echo "Uso: ./exec.sh <script_de_prueba> <worker_id>"
    exit 1
fi

SCRIPT_NAME=$(echo "$1" | tr '[:upper:]' '[:lower:]')
WORKER_ID="$2"

# Validación del ID del worker
if ! [[ "$WORKER_ID" =~ ^[0-9]+$ ]]; then
    echo "ERROR: El worker_id debe ser un número entero."
    exit 1
fi

SCRIPT_PATH="./scripts/${SCRIPT_NAME}.sh"

# Verificar si el script existe
if [ ! -f "$SCRIPT_PATH" ]; then
    echo "ERROR: El script '$SCRIPT_PATH' no existe."
    exit 1
fi

echo "Ejecutando script: $SCRIPT_PATH con worker_id=$WORKER_ID"
chmod +x "$SCRIPT_PATH"

# Ejecutar el segundo script pasándole el worker_id
"$SCRIPT_PATH" "$WORKER_ID"

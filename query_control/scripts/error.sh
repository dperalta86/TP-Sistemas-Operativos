#!/bin/bash

# Primer parámetro que recibe el script
QC_ID=$1

case "$QC_ID" in
  1)
    SCRIPT=ESCRITURA_ARCHIVO_COMMITED
    PRIORIDAD=1   
    ;;
  2)
    SCRIPT=FILE_EXISTENTE
    PRIORIDAD=1
    ;;
  3)
    SCRIPT=LECTURA_FUERA_DEL_LIMITE
    PRIORIDAD=1
    ;;
  4)
    SCRIPT=TAG_EXISTENTE
    PRIORIDAD=1
    ;;
  *)
    echo "Error: query control desconocido '$QC_ID'"
    exit 1
    ;;
esac

SCRIPT_PATH="./scripts/apply_config.sh"

chmod +x "$SCRIPT_PATH"

# Llama a apply_config.sh con los valores comunes
"$SCRIPT_PATH" "$QC_ID" "$SCRIPT" "$PRIORIDAD"

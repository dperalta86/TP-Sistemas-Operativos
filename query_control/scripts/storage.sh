#!/bin/bash

# Primer parámetro que recibe el script
QC_ID=$1

case "$QC_ID" in
  1)
    SCRIPT=STORAGE_1
    PRIORIDAD=0   
    ;;
  2)
    SCRIPT=STORAGE_2
    PRIORIDAD=2
    ;;
  3)
    SCRIPT=STORAGE_3
    PRIORIDAD=4
    ;;
  4)
    SCRIPT=STORAGE_4
    PRIORIDAD=6
    ;;
  5)
    SCRIPT=STORAGE_5
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

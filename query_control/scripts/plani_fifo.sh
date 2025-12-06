#!/bin/bash

# Primer parámetro que recibe el script
QC_ID=$1

case "$QC_ID" in
  1)
    SCRIPT=FIFO_1
    PRIORIDAD=4   
    ;;
  2)
    SCRIPT=FIFO_2
    PRIORIDAD=3
    ;;
  3)
    SCRIPT=FIFO_3
    PRIORIDAD=5
    ;;
  4)
    SCRIPT=FIFO_4
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

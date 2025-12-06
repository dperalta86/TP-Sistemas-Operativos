#!/bin/bash

# Primer parámetro que recibe el script
WORKER_ID=$1

case "$WORKER_ID" in
  1)
    TAM_MEMORIA=1024
    RETARDO_MEMORIA=50
    ALGORITMO="LRU"
    ;;
  2)
    TAM_MEMORIA=1024
    RETARDO_MEMORIA=50
    ALGORITMO="CLOCK-M"
    ;;
  *)
    echo "Error: worker desconocido '$WORKER_ID'"
    exit 1
    ;;
esac

SCRIPT_PATH="./scripts/apply_config.sh"

chmod +x "$SCRIPT_PATH"

# Llama a apply_config.sh con los valores comunes
"$SCRIPT_PATH" "$WORKER_ID" "$TAM_MEMORIA" "$RETARDO_MEMORIA" "$ALGORITMO"

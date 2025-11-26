#!/bin/bash

# Primer parámetro que recibe el script
WORKER_ID=$1

case "$WORKER_ID" in
  1)
    TAM_MEMORIA=128
    RETARDO_MEMORIA=100
    ALGORITMO_REEMPLAZO=LRU    
    ;;
  2)
    TAM_MEMORIA=256
    RETARDO_MEMORIA=50
    ALGORITMO_REEMPLAZO=CLOCK-M
    ;;
  3)
    TAM_MEMORIA=64
    RETARDO_MEMORIA=150
    ALGORITMO_REEMPLAZO=CLOCK-M
    ;;
  4)
    TAM_MEMORIA=96
    RETARDO_MEMORIA=125
    ALGORITMO_REEMPLAZO=LRU
    ;;
  5)
    TAM_MEMORIA=160
    RETARDO_MEMORIA=75
    ALGORITMO_REEMPLAZO=LRU
    ;;
  6)
    TAM_MEMORIA=192
    RETARDO_MEMORIA=175
    ALGORITMO_REEMPLAZO=CLOCK-M
    ;;
  *)
    echo "Error: worker desconocido '$WORKER_ID'"
    exit 1
    ;;
esac

SCRIPT_PATH="./scripts/apply_config.sh"

chmod +x "$SCRIPT_PATH"

# Llama a apply_config.sh con los valores comunes
"$SCRIPT_PATH" "$WORKER_ID" "$TAM_MEMORIA" "$RETARDO_MEMORIA" "$ALGORITMO_REEMPLAZO"

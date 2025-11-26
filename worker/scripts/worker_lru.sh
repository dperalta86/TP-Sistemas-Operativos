#!/bin/bash

# Primer parámetro que recibe el script
WORKER_ID=$1

TAM_MEMORIA=48
RETARDO_MEMORIA=250
ALGORITMO="LRU"

SCRIPT_PATH="./scripts/apply_config.sh"

chmod +x "$SCRIPT_PATH"

# Llama a apply_config.sh con los valores comunes
"$SCRIPT_PATH" "$WORKER_ID" "$TAM_MEMORIA" "$RETARDO_MEMORIA" "$ALGORITMO"

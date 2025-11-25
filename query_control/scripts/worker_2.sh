#!/bin/bash

QC_ID=$1

SCRIPT=MEMORIA_WORKER_2
PRIORIDAD=0

SCRIPT_PATH="./scripts/apply_config.sh"

chmod +x "$SCRIPT_PATH"

# Llama a apply_config.sh con los valores comunes
"$SCRIPT_PATH" "$QC_ID" "$SCRIPT" "$PRIORIDAD"

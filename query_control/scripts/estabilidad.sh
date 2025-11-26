#!/bin/bash

# Primer parámetro que recibe el script
QC_ID=$1

SCRIPT_PATH="./scripts/apply_config.sh"
chmod +x "$SCRIPT_PATH"

for i in $(seq 1 4); do 
    SCRIPT=AGING_${i}
    PRIORIDAD=20   

    for j in $(seq 1 25); do
        "$SCRIPT_PATH" "$QC_ID" "$SCRIPT" "$PRIORIDAD" &
    done
done
#!/bin/bash

QC_ID="$1"
SCRIPT="$2"
PRIORIDAD="$3"

if [ -z "$QC_ID" ]; then
    echo "Uso: $0 <query_control_id> <script> <prioridad>"
    exit 1
fi

if ! [[ "$QC_ID" =~ ^[0-9]+$ ]]; then
    echo "ERROR: El ID de query_control debe ser un número."
    exit 1
fi

CONFIG_PATH="./src/config/query_control.config"

echo "--------------------------------------"
echo "Ejecutando QUERY_CONTROL: $SCRIPT"
echo "--------------------------------------"

./bin/query_control "$CONFIG_PATH" "$SCRIPT" "$PRIORIDAD"

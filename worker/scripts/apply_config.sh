#!/bin/bash

# ============================================================================
# Script: apply_config.sh (para módulo WORKER)
# Descripción:
#   - Recibe el ID del worker como parámetro
#   - Edita el archivo de configuración correspondiente
#   - Compila el módulo WORKER
#   - Ejecuta la instancia indicada del worker
# ============================================================================

WORKER_ID="$1"
TAM_MEMORIA="$2"
RETARDO_MEMORIA="$3"
ALGORITMO_REEMPLAZO="$4"

if [ -z "$WORKER_ID" ]; then
    echo "Uso: $0 <id_worker>"
    exit 1
fi

if ! [[ "$WORKER_ID" =~ ^[0-9]+$ ]]; then
    echo "ERROR: El ID de worker debe ser un número."
    exit 1
fi

CONFIG_PATH="./config/example.config"

if [ ! -f "$CONFIG_PATH" ]; then
    echo "ERROR: No existe el archivo de configuración '$CONFIG_PATH'"
    exit 1
fi

echo "Editando configuración de WORKER $WORKER_ID..."

sed -i "s/^TAM_MEMORIA=.*/TAM_MEMORIA=$TAM_MEMORIA/" "$CONFIG_PATH"
sed -i "s/^RETARDO_MEMORIA=.*/RETARDO_MEMORIA=$RETARDO_MEMORIA/" "$CONFIG_PATH"
sed -i "s/^ALGORITMO_REEMPLAZO=.*/ALGORITMO_REEMPLAZO=$ALGORITMO_REEMPLAZO/" "$CONFIG_PATH"

echo "Configuración aplicada para worker $WORKER_ID:"
grep -E "TAM_MEMORIA|RETARDO_MEMORIA|ALGORITMO_REEMPLAZO" "$CONFIG_PATH"

echo "--------------------------------------"
echo "Ejecutando WORKER $WORKER_ID..."
echo "--------------------------------------"

./bin/worker "$CONFIG_PATH" "$WORKER_ID"

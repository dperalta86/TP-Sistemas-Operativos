#!/bin/bash

RETARDO_OPERACION="$1"
RETARDO_ACCESO_BLOQUE="$2"
FRESH_START="$3"

CONFIG_PATH="./src/config/storage.config"

if [ ! -f "$CONFIG_PATH" ]; then
    echo "ERROR: No existe el archivo de configuración '$CONFIG_PATH'"
    exit 1
fi

echo "Editando configuración de STORAGE"

sed -i "s/^OPERATION_DELAY=.*/OPERATION_DELAY=$RETARDO_OPERACION/" "$CONFIG_PATH"
sed -i "s/^BLOCK_ACCESS_DELAY=.*/BLOCK_ACCESS_DELAY=$RETARDO_ACCESO_BLOQUE/" "$CONFIG_PATH"
sed -i "s/^FRESH_START=.*/FRESH_START=$FRESH_START/" "$CONFIG_PATH"

echo "Configuración aplicada para storage:"
grep -E "OPERATION_DELAY|BLOCK_ACCESS_DELAY|FRESH_START" "$CONFIG_PATH"

echo "--------------------------------------"
echo "Ejecutando STORAGE"
echo "--------------------------------------"

./bin/storage "$CONFIG_PATH"

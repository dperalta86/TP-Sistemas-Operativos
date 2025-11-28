#!/bin/bash

CONFIG_PATH="$1"
ALGORITMO="$2"
AGING="$3"

if [ -z "$CONFIG_PATH" ] || [ -z "$ALGORITMO" ] || [ -z "$AGING" ]; then
    echo "Uso: .$0 <config_path> <ALGORITMO_PLANIFICACION> <TIEMPO_AGING>"
    exit 1
fi

if [ ! -f "$CONFIG_PATH" ]; then
    echo "ERROR: El archivo '$CONFIG_PATH' no existe."
    exit 1
fi

echo "Editando configuración en: $CONFIG_PATH"

sed -i "s/^ALGORITMO_PLANIFICACION=.*/ALGORITMO_PLANIFICACION=$ALGORITMO/" "$CONFIG_PATH"
sed -i "s/^TIEMPO_AGING=.*/TIEMPO_AGING=$AGING/" "$CONFIG_PATH"

echo "Config actualizado:"
echo "   ALGORITMO_PLANIFICACION=$ALGORITMO"
echo "   TIEMPO_AGING=$AGING"

echo "--------------------------------------"
echo "Ejecutando MASTER..."
echo "--------------------------------------"

./bin/master "$CONFIG_PATH"

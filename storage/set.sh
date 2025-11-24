#!/bin/bash

CONFIG_FILE="./src/config/storage.config"

# Verificación de argumentos
if [ $# -ne 1 ]; then
  echo "Uso: $0 <STORAGE_IP>"
  exit 1
fi

STORAGE_IP="$1"

# Verificar existencia del archivo
if [ ! -f "$CONFIG_FILE" ]; then
  echo "ERROR: No se encontró el archivo $CONFIG_FILE"
  echo "Abortando."
  exit 1
fi

# Verificar que la clave existe
if ! grep -q "^STORAGE_IP=" "$CONFIG_FILE"; then
  echo "ERROR: La clave 'STORAGE_IP' no existe en el archivo."
  echo "Abortando."
  exit 1
fi

# Reemplazar el valor de la clave
sed -i "s|^STORAGE_IP=.*|STORAGE_IP=${STORAGE_IP}|" "$CONFIG_FILE"

echo "Archivo storage.config actualizado correctamente."

#!/bin/bash

CONFIG_FILE="./src/config/master.config"

# Verificación de argumentos
if [ $# -ne 1 ]; then
  echo "Uso: $0 <IP_ESCUCHA>"
  exit 1
fi

IP_ESCUCHA="$1"

# Verificar existencia del archivo
if [ ! -f "$CONFIG_FILE" ]; then
  echo "ERROR: No se encontró el archivo $CONFIG_FILE"
  echo "Abortando."
  exit 1
fi

# Verificar que la clave existe
if ! grep -q "^IP_ESCUCHA=" "$CONFIG_FILE"; then
  echo "ERROR: La clave 'IP_ESCUCHA' no existe en el archivo."
  echo "Abortando."
  exit 1
fi

# Reemplazar el valor
sed -i "s|^IP_ESCUCHA=.*|IP_ESCUCHA=${IP_ESCUCHA}|" "$CONFIG_FILE"

echo "Archivo master.config actualizado correctamente."

#!/bin/bash

CONFIG_FILE="./config/example.config"

# Verificación de argumentos
if [ $# -ne 2 ]; then
  echo "Uso: $0 <IP_MASTER> <IP_STORAGE>"
  exit 1
fi

IP_MASTER="$1"
IP_STORAGE="$2"

# Verificar existencia del archivo
if [ ! -f "$CONFIG_FILE" ]; then
  echo "ERROR: No se encontró el archivo $CONFIG_FILE"
  echo "Abortando."
  exit 1
fi

# Función para modificar una clave existente
set_config_value() {
  local key="$1"
  local value="$2"

  if grep -q "^${key}=" "$CONFIG_FILE"; then
    sed -i "s|^${key}=.*|${key}=${value}|" "$CONFIG_FILE"
  else
    echo "ERROR: La clave '${key}' no existe en el archivo."
    echo "Abortando."
    exit 1
  fi
}

set_config_value "IP_MASTER" "$IP_MASTER"
set_config_value "IP_STORAGE" "$IP_STORAGE"

echo "Archivo .config actualizado correctamente."


echo "--------------------------------------"
echo "Compilando módulo WORKER"
echo "--------------------------------------"

make clean all
if [ $? -ne 0 ]; then
    echo "Error en la compilación."
    exit 1
fi

echo "Compilación exitosa."
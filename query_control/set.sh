#!/bin/bash

CONFIG_FILE="./src/config/query_control.config"

# Verificación de argumentos
if [ $# -ne 1 ]; then
  echo "Uso: $0 <IP_MASTER>"
  exit 1
fi

IP_MASTER="$1"

# Verificar existencia del archivo
if [ ! -f "$CONFIG_FILE" ]; then
  echo "ERROR: No se encontró el archivo $CONFIG_FILE"
  echo "Abortando."
  exit 1
fi

# Función para setear o reemplazar una clave en el archivo
set_config_value() {
  local key="$1"
  local value="$2"

  if grep -q "^${key}=" "$CONFIG_FILE"; then
    # Reemplazar la línea existente
    sed -i "s|^${key}=.*|${key}=${value}|" "$CONFIG_FILE"
  else
    # Agregar al final del archivo
    echo "${key}=${value}" >> "$CONFIG_FILE"
  fi
}

set_config_value "IP_MASTER" "$IP_MASTER"

echo "Archivo actualizado:"
cat "$CONFIG_FILE"

echo "--------------------------------------"
echo "Compilando módulo QUERY_CONTROL"
echo "--------------------------------------"

make clean all
if [ $? -ne 0 ]; then
    echo "Error en la compilación."
    exit 1
fi

echo "Compilación exitosa."

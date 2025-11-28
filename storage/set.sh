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

echo "--------------------------------------"
echo "Creando superblock"
echo "--------------------------------------"

MOUNT_POINT=$(grep "^MOUNT_POINT=" "$CONFIG_FILE" | cut -d '=' -f2)

if [ -z "$MOUNT_POINT" ]; then
    echo "ERROR: No se encontró MOUNT_POINT en $CONFIG_FILE"
    exit 1
fi

# Crear carpeta por si no existe
mkdir -p "$MOUNT_POINT"

# Ruta del archivo superblock
SUPERBLOCK_PATH="$MOUNT_POINT/superblock.config"

echo "Creando superblock en: $SUPERBLOCK_PATH"

# Crear o sobrescribir el archivo
cat > "$SUPERBLOCK_PATH" <<EOF
FS_SIZE=65536
BLOCK_SIZE=16
EOF

echo "Superblock creado:"
cat "$SUPERBLOCK_PATH"

echo "--------------------------------------"
echo "Compilando módulo STORAGE"
echo "--------------------------------------"

make clean all
if [ $? -ne 0 ]; then
    echo "Error en la compilación."
    exit 1
fi

echo "Compilación exitosa."

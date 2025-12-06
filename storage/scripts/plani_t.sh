#!/bin/bash

RETARDO_OPERACION=25
RETARDO_ACCESO_BLOQUE=25
FRESH_START=TRUE

SCRIPT_PATH="./scripts/apply_config.sh"

chmod +x "$SCRIPT_PATH"

# Llama a apply_config.sh con los valores comunes
"$SCRIPT_PATH" "$RETARDO_OPERACION" "$RETARDO_ACCESO_BLOQUE" "$FRESH_START"

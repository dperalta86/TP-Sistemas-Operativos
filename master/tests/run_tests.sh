#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "🔍 Ejecutando todos los tests..."

make test

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Todos los tests pasaron correctamente.${NC}"
else
    echo -e "${RED}❌ Algunos tests fallaron.${NC}"
fi

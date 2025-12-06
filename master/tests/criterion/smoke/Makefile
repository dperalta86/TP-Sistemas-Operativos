# tests/criterion/smoke/Makefile
CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lcriterion

TARGET = test_smoke
SRC = test_smoke.c

all: $(TARGET)

$(TARGET): $(SRC)
	@echo "Compilando test smoke..."
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)
	@echo "✅ Compilación exitosa"

test: $(TARGET)
	@echo ""
	@echo "🧪 Ejecutando test smoke..."
	@echo "================================"
	./$(TARGET)

verbose: $(TARGET)
	@echo ""
	@echo "🧪 Ejecutando test smoke (verbose)..."
	@echo "================================"
	./$(TARGET) --verbose

# Ejecutar solo un test específico
test-sanity: $(TARGET)
	./$(TARGET) --filter=smoke/sanity_check

# Ver lista de tests
list: $(TARGET)
	./$(TARGET) --list

clean:
	rm -f $(TARGET)

.PHONY: all test verbose test-sanity list clean
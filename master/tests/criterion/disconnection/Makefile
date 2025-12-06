# tests/criterion/unit/Makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -DTEST_ENVIRONMENT \
         -I../../../src \
         -I../../../../utils/src \
         -I../../../../utils/src/connection \
         -I../../helpers

# Detectar ubicación de libcriterion
CRITERION_LIBDIR = $(shell pkg-config --variable=libdir criterion 2>/dev/null || echo "/usr/local/lib")

LIBS = -lcriterion -lcommons -lpthread -lcrypto
LDFLAGS = -Wl,-rpath,$(CRITERION_LIBDIR)

TARGETS = test_disconnection

# Fuentes del proyecto necesarias
PROJECT_SRC = \
    ../../../src/query_control_manager.c \
	../../../src/worker_manager.c \
    ../../../../utils/src/connection/serialization.c \
	../../../src/scheduler.c \
	../../../src/init_master.c \
	../../../src/aging.c \
	../../../src/disconnection_handler.c \
	../../../src/config/master_config.c \
    ../../helpers/test_helpers.c

all: $(TARGETS)

test_disconnection: test_disconnection.c
	@echo "Compilando test_disconnection..."
	$(CC) $(CFLAGS) test_disconnection.c $(PROJECT_SRC) -o test_disconnection $(LDFLAGS) $(LIBS)


test: all
	@echo ""
	@echo "🧪 Ejecutando test_disconnection..."
	@echo "===================================="
	./test_disconnection


verbose: all
	@for test in $(TARGETS); do \
		echo ""; \
		echo "🧪 Ejecutando $$test (verbose)..."; \
		echo "===================================="; \
		./$$test --verbose; \
	done

valgrind: all
	@for test in $(TARGETS); do \
		echo ""; \
		echo "🔍 Valgrind en $$test..."; \
		echo "========================"; \
		valgrind --leak-check=full \
		         --show-leak-kinds=all \
		         --track-origins=yes \
		         --error-exitcode=1 \
		         ./$$test; \
	done

clean:
	rm -f $(TARGETS) *.log

.PHONY: all test verbose valgrind clean
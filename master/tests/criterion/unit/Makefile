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

TARGETS = test_initialization test_query_management test_worker_management test_cleanup 

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

test_initialization: test_initialization.c
	@echo "Compilando test_initialization..."
	$(CC) $(CFLAGS) test_initialization.c $(PROJECT_SRC) -o test_initialization $(LDFLAGS) $(LIBS)

test_query_management: test_query_management.c
	@echo "Compilando test_query_management..."
	$(CC) $(CFLAGS) test_query_management.c $(PROJECT_SRC) -o test_query_management $(LDFLAGS) $(LIBS)

test_worker_management: test_worker_management.c
	@echo "Compilando test_worker_management"
	$(CC) $(CFLAGS) test_worker_management.c $(PROJECT_SRC) -o test_worker_management $(LDFLAGS) $(LIBS)

test_cleanup: test_cleanup.c
	@echo "Compilando test_cleanup..."
	$(CC) $(CFLAGS) test_cleanup.c $(PROJECT_SRC) -o test_cleanup $(LDFLAGS) $(LIBS)


test: all
	@echo ""
	@echo "🧪 Ejecutando test_initialization..."
	@echo "===================================="
	./test_initialization
	@echo ""
	@echo "🧪 Ejecutando test_query_management..."
	@echo "======================================="
	./test_query_management
	@echo ""
	@echo "🧪 Ejecutando test_worker_management..."
	@echo "======================================="
	./test_worker_management
	@echo ""
	@echo "🧪 Ejecutando test_cleanup..."
	@echo "======================================="
	./test_cleanup

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
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

TARGETS = test_aging_simple test_aging_concurrent test_deadlock_detection test_scheduler_integration

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

test_aging_simple: test_aging_simple.c
	@echo "Compilando test_aging_simple..."
	$(CC) $(CFLAGS) test_aging_simple.c $(PROJECT_SRC) -o test_aging_simple $(LDFLAGS) $(LIBS)

test_aging_concurrent: test_aging_concurrent.c
	@echo "Compilando test_aging_concurrent..."
	$(CC) $(CFLAGS) test_aging_concurrent.c $(PROJECT_SRC) -o test_aging_concurrent $(LDFLAGS) $(LIBS)

test_deadlock_detection: test_deadlock_detection.c
	@echo "Compilando test_deadlock_detection..."
	$(CC) $(CFLAGS) test_deadlock_detection.c $(PROJECT_SRC) -o test_deadlock_detection $(LDFLAGS) $(LIBS)

test_scheduler_integration: test_scheduler_integration.c
	@echo "Compilando test_agitest_scheduler_integrationng..."
	$(CC) $(CFLAGS) test_scheduler_integration.c $(PROJECT_SRC) -o test_scheduler_integration $(LDFLAGS) $(LIBS)


test: all
	@echo ""
	@echo "🧪 Ejecutando test_aging_simple..."
	@echo "===================================="
	./test_aging_simple
	@echo ""
	@echo "🧪 Ejecutando test_aging_concurrent..."
	@echo "===================================="
	./test_aging_concurrent
	@echo ""
	@echo "🧪 Ejecutando test_deadlock_detection..."
	@echo "======================================="
	./test_deadlock_detection
	@echo ""
	@echo "🧪 Ejecutando test_scheduler_integration..."
	@echo "======================================="
	./test_scheduler_integration

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
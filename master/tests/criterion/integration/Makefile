# tests/criterion/integration/Makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -DTEST_ENVIRONMENT \
         -I../../../src \
         -I../../../../utils/src \
         -I../../../../utils/src/connection \
         -I../../helpers

CRITERION_LIBDIR = $(shell pkg-config --variable=libdir criterion 2>/dev/null || echo "/usr/local/lib64")

LIBS = -lcriterion -lcommons -lpthread -lcrypto
LDFLAGS = -Wl,-rpath,$(CRITERION_LIBDIR)

TARGETS = test_fifo_scheduling test_priority_scheduling

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

test_fifo_scheduling: test_fifo_scheduling.c
	@echo "Compilando test_fifo_scheduling..."
	$(CC) $(CFLAGS) test_fifo_scheduling.c $(PROJECT_SRC) -o test_fifo_scheduling $(LDFLAGS) $(LIBS)

test_priority_scheduling: test_priority_scheduling.c
	@echo "Compilando test_priority_scheduling..."
	$(CC) $(CFLAGS) test_priority_scheduling.c $(PROJECT_SRC) -o test_priority_scheduling $(LDFLAGS) $(LIBS)

test: all
	@echo ""
	@echo "🧪 Ejecutando test_fifo_scheduling..."
	@echo "======================================"
	./test_fifo_scheduling
	@echo ""
	@echo "🧪 Ejecutando test_priority_scheduling..."
	@echo "=========================================="
	./test_priority_scheduling

verbose: all
	./test_fifo_scheduling --verbose
	./test_priority_scheduling --verbose

clean:
	rm -f $(TARGETS) *.log

.PHONY: all test verbose clean
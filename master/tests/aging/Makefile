include ../Makefile.common

TARGET = test_aging
SRC = test_aging.c

all: $(TARGET)

# Fuentes reales que el test necesita
PROJECT_SRC = \
    ../../src/aging.c \
    ../../src/query_control_manager.c \
    ../../src/scheduler.c \
    ../../../utils/src/connection/serialization.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(PROJECT_SRC) ../unity/src/unity.c \
	    -lcommons -lpthread -lcrypto -o $(TARGET)


clean:
	rm -f $(TARGET) *.o

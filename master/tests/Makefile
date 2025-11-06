CC = gcc
CFLAGS = -Wall -Wextra -g \
	-I../src \
	-I../../utils/src \
	-I../../utils/src/connection \
	-I./unity/src

LIBS = -lcommons -lpthread -lcrypto

SRCS = \
	test_aging.c \
	unity/src/unity.c \
	../src/query_control_manager.c \
	../src/scheduler.c \
	../src/aging.c \
	../../utils/src/connection/serialization.c

OBJS = $(SRCS:.c=.o)

test_aging: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LIBS)

clean:
	rm -f $(OBJS) test_aging

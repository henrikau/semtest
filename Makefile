# Makefile for semtest
CC=gcc
CFLAGS=-Wall -D_GNU_SOURCE
LDFLAGS=-lpthread -lrt

TARGET=semtest
DEFAULT: rebuild
.PHONY: clean all

%.o: %.c
	$(CC) -c $< $(CFLAGS)

$(TARGET): semtest.o main.o
	$(CC) -o $(TARGET) $^ $(LDFLAGS)

rebuild: clean all

all: $(TARGET)

clean:
	rm -f $(TARGET) *.o

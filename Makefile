# Makefile for semtest

CFLAGS=-Wall -D_GNU_SOURCE
LDFLAGS=-lpthread -lrt

# make it possible to make with CROSS_COMPILE as a make-flag
# i.e. make CROSS_COMPILE=arm-
CC=$(CROSS_COMPILE)gcc

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

mrproper: clean
	rm -f *~

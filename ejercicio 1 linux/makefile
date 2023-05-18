CC = gcc
CFLAGS = -Wall -pthread

all: index

index: src/index.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f index
# Andréas Guillot

CFLAGS := -Wall -Wextra -Werror -g
LDFLAGS := -pthread
CC := gcc

all: travailleur patron

travailleur: travailleur.o fifo.o ipc.o
	$(CC) -o travailleur travailleur.o fifo.o ipc.o $(CFLAGS) $(LDFLAGS)

travailleur.o: travailleur.c
	$(CC) -c travailleur.c $(CFLAGS) $(LDFLAGS)

ipc.o: ipc.c ipc.h
	$(CC) -c ipc.c $(CFLAGS)

fifo.o: fifo.c fifo.h
	$(CC) -c fifo.c $(CFLAGS) $(LDFLAGS)

patron: patron.o
	$(CC) -o patron patron.o $(CFLAGS)

patron.o: patron.c
	$(CC) -c patron.c $(CFLAGS)

clean:
	rm -rf *.o travailleur patron

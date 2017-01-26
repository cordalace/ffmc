CC = gcc
CFLAGS = -g -Wall -O2

all: ffmc

ffmc: main.o
	$(CC) $(CFLAGS) -o ffmc main.o

main.o: main.c
	$(CC) -c main.c

clean:
	rm ffmc *.o

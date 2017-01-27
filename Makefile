CC = gcc
LDFLAGS =
CFLAGS := -Wall $(shell pkg-config fuse --cflags) $(shell curl-config --cflags)
LDLIBS := $(shell pkg-config fuse --libs) $(shell curl-config --libs)

all: ffmc

ffmc: main.o
	$(CC) $(LDFLAGS) -o ffmc main.o $(LDLIBS)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -rf ffmc *.o

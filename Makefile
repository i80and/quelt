CC=gcc

DEBUG=-g -O0
PRODUCTION=-Os
# Change to DEBUG to include debugging symbols
PROFILE=${PRODUCTION}
CFLAGS=-Wall -Wextra -Wshadow -pedantic -std=c99 -Wno-unused-parameter -D _FILE_OFFSET_BITS=64 ${PROFILE}

all: quelt quelt-split

quelt: quelt.c pprint.o
	$(CC) $(CFLAGS) quelt.c pprint.o -o quelt

quelt-split: quelt-split.c pprint.o
	$(CC) $(CFLAGS) quelt-split.c pprint.o -o quelt-split -lexpat -lz

pprint.o: pprint.c pprint.h
	$(CC) $(CFLAGS) pprint.c -c

clean:
	rm -f quelt quelt-split *.o

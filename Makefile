CC=gcc

DEBUG=-g -O0
PRODUCTION=-Os
# Change to DEBUG to include debugging symbols
PROFILE=${PRODUCTION}
CFLAGS=-Wall -Wextra -Wshadow -pedantic -std=c99 -Wno-unused-parameter -D _FILE_OFFSET_BITS=64 ${PROFILE}

all: quelt quelt-split

quelt: quelt.c quelt-common.o pprint.o
	$(CC) $(CFLAGS) quelt.c quelt-common.o pprint.o -o quelt

quelt-split: quelt-split.c quelt-common.o pprint.o
	$(CC) $(CFLAGS) quelt-split.c quelt-common.o pprint.o -o quelt-split -lexpat -lz

quelt-common.o: quelt-common.c quelt-common.h pprint.h
	$(CC) $(CFLAGS) quelt-common.c -c

pprint.o: pprint.c pprint.h
	$(CC) $(CFLAGS) pprint.c -c

clean:
	rm -f quelt quelt-split *.o

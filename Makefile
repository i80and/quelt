CC=gcc

DEBUG=-g -O0
PRODUCTION=-Os
# Change to DEBUG to include debugging symbols
PROFILE=${DEBUG}
CFLAGS=-Wall -Wextra -Wshadow -pedantic -std=c99 -Wno-unused-parameter -D _FILE_OFFSET_BITS=64 ${PROFILE}

all: quelt quelt-split

quelt: src/quelt.c src/quelt-common.o src/database.o
	$(CC) $(CFLAGS) src/quelt.c src/quelt-common.o src/database.o -o quelt -lz

quelt-split: src/quelt-split.c src/quelt-common.o src/database.o
	$(CC) $(CFLAGS) src/quelt-split.c src/quelt-common.o src/database.o -o quelt-split -lexpat -lz

src/quelt-common.o: src/quelt-common.c src/quelt-common.h
	$(CC) $(CFLAGS) -c -o $@ src/quelt-common.c

src/database.o: src/database.h src/database.c
	$(CC) $(CFLAGS) -c -o $@ src/database.c

clean:
	rm -f quelt quelt-split src/*.o

// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#ifndef PPRINT_H
#define PPRINT_H

#include <stdint.h>
#include <stdio.h>

typedef uint8_t pp_flags_t;
#define COLOR_NORMAL 0x00
#define COLOR_BRIGHT 0x01
#define COLOR_UNDERLINE 0x02
#define COLOR_GREEN 0x04
#define COLOR_YELLOW 0x08
#define COLOR_RED 0x10
#define COLOR_BLUE 0x20

// Print a message given an ORed set of formatting flags.
void pp_fprint(FILE* stream, const pp_flags_t flags, const char* msg);

// Convenience macro calling pp_fprint with stdout
#define pp_print(flags, msg) pp_fprint(stdout, (flags), (msg))

// Reset any escape sequences on the given stream
void pp_reset(FILE* stream);

// Print a message giving the file and line of the call
#define log(msg) fprintf(stderr, "%s[%d] %s\n", __FILE__, __LINE__, (msg))
#define log_printf(msg, ...) {fprintf(stderr, "%s[%d] ", __FILE__, __LINE__); \
	fprintf(stderr, (msg), __VA_ARGS__); \
	fprintf(stderr, "\n");}
#endif

// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#include "pprint.h"

#define RESET_TOKEN "\x1b[0m"
#define LONGEST_PREFIX 7

void pp_reset(FILE* stream) {
	fputs(RESET_TOKEN, stream);
}

// There's a legit argument that this should accept printf-style input
void pp_fprint(FILE* stream, const pp_flags_t flags, const char* msg) {
	char prefix[LONGEST_PREFIX];
	char* cursor = prefix;
	
	if(flags & COLOR_GREEN) {
		cursor[0] = '3';
		cursor[1] = '2';
		cursor[2] = ';';
		cursor += 3;
	}
	else if(flags & COLOR_YELLOW) {	
		cursor[0] = '3';
		cursor[1] = '3';
		cursor[2] = ';';
		cursor += 3;
	}
	else if(flags & COLOR_RED) {
		cursor[0] = '3';
		cursor[1] = '1';
		cursor[2] = ';';
		cursor += 3;
	}
	else if(flags & COLOR_BLUE) {
		cursor[0] = '3';
		cursor[1] = '4';
		cursor[2] = ';';
		cursor += 3;
	}
	
	if(flags & COLOR_BRIGHT) {
		cursor[0] = '1';
		cursor[1] = ';';
		cursor += 2;
	}

	if(flags & COLOR_UNDERLINE) {
		cursor[0] = '4';
		cursor[1] = ';';
		cursor += 2;
	}
	
	// Overwrite the trailing semicolon with a null byte to create a string
	cursor[-1] = '\0';
	fprintf(stream, "\x1b[%sm%s%s", prefix, msg, RESET_TOKEN);
}

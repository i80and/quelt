// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#include <stdbool.h>
#include <string.h>
#include "quelt-common.h"
#include "pprint.h"

static bool option_scan = false;
static bool option_plain = false;

typedef struct {
	char title[255];
	f_offset start;
} QueltField;

void search(const char* article) {
	
}

void parse_argument(const char* arg) {
	if(!option_scan && strcmp(arg, "--scan") == 0) {
		option_scan = true;
	}
	else if(!option_plain && strcmp(arg, "--plain") == 0) {
		option_plain = true;
	}
}

int main(int argc, char** argv) {
	if(argc < 2) {
		log("No article specified\nUsage: quelt article [--scan] [--plain]");
		return 1;
	}

	const char* article = argv[1];

	for(int i = 2; i < argc; i+=1) {
		parse_argument(argv[i]);
	}

	search(article);

	return 0;
}

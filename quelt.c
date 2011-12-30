// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <string.h>
#include "database.h"
#include "quelt-common.h"
#include "pprint.h"

static bool option_search = false;
static bool option_plain = false;

#define RETURN_OK 0
#define RETURN_UNKNOWNERROR 1
#define RETURN_NOMATCH 2

void search_match_handler(void* ctx, char* title, size_t title_len) {
	printf("%s\n", title);
}

// Perform a linear-time search 
static void search(QueltDB* db, const char* title) {
	queltdb_search(db, title, &search_match_handler, NULL);
}

void parse_argument(const char* arg) {
	if(!option_search && strcmp(arg, "--search") == 0) {
		option_search = true;
	}
	else if(!option_plain && strcmp(arg, "--plain") == 0) {
		option_plain = true;
	}
}

int main(int argc, char** argv) {
	if(argc < 2) {
		log("No article specified\nUsage: quelt article [--search] [--plain]");
		return 1;
	}

	const char* article = argv[1];

	for(int i = 2; i < argc; i+=1) {
		parse_argument(argv[i]);
	}

	QueltDB* db = queltdb_open();

	if(option_search) {
		search(db, article);
	}
	else {
		fail(RETURN_NOMATCH, "Normal operation unimplemented.");
	}

	queltdb_close(db);

	return 0;
}

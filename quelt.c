// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#include <stdbool.h>
#include <string.h>
#include "pprint.h"

void parse(const char* article) {
	
}

int main(int argc, char** argv) {
	if(argc < 2) {
		log("No article specified\nUsage: quelt article [--plain]");
		return 1;
	}

	const char* article = argv[1];
	parse(article);

	return 0;
}

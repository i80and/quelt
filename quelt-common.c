// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#include <stdlib.h>
#include "pprint.h"

static int return_status = 0;

void set_return_flag(int flag) {
	return_status |= flag;
}

void fail(int flag, const char* msg) {
	log(msg);
	set_return_flag(flag);
	exit(return_status);
}
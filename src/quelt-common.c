// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#include <stdlib.h>
#include "pprint.h"

void fail(int flag, const char* msg) {
    if(msg != NULL) {
        log(msg);
    }
    exit(flag);
}

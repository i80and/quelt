// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#ifndef QUELTCOMMON_H
#define QUELTCOMMON_H

#include <stdint.h>

// Add a flag to our return code.
void set_return_flag(int flag);

// Early exit with our return flags
void fail(int flag, const char* msg);

#endif

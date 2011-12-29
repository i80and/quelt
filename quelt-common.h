// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#ifndef QUELTCOMMON_H
#define QUELTCOMMON_H

#include <stdint.h>
// Technically this should be off_t, but we always want it to be 64-bit
typedef int64_t f_offset;
// Compatibility shim for Windows
#ifdef _WIN32
# define ftello _ftelli64
#endif

// Maximum length in bytes of a Wikimedia page title
#define MAX_TITLE_LEN 255

// Add a flag to our return code.
void set_return_flag(int flag);

// Early exit with our return flags
void fail(int flag, const char* msg);

#endif

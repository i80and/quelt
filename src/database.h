// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#ifndef QUELT_DATABASE_H
#define QUELT_DATABASE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Maximum length in bytes of a Wikimedia page title
#define MAX_TITLE_LEN 255

// Technically this should be off_t, but we always want it to be 64-bit
typedef int64_t f_offset;

// Opaque handle for database contexts
typedef struct QueltDB QueltDB;

// Handler for read events
typedef void(*queltdb_handler_func)(void* ctx, char* chunk, size_t chunk_len);

// Create a new database.  Segment length is used to split the database into
// equally sized "search segments", or 0 to indicate that the whole database
// is a single segment.
QueltDB* queltdb_create(int segment_length);

// Write a chunk of bytes to the database
void queltdb_writechunk(QueltDB* db, const char* buf, size_t len);

// Give an index record for the preceeding chunks
void queltdb_finisharticle(QueltDB* db, const char* title, size_t len);

// Open a database for reading
QueltDB* queltdb_open(void);

// Return the number of articles in this database
int queltdb_narticles(const QueltDB* db);

// Search for any article containing the given needle.  For each match,
// call handler(ctx, match_title, title_len)
void queltdb_search(QueltDB* db, const char* needle,
					queltdb_handler_func handler, void* ctx);

// Quickly find the given article, and call handler(ctx, chunk, chunk_len) for
// each chunk of the article as it becomes available
int queltdb_getarticle(QueltDB* db, const char* title,
						queltdb_handler_func handler, void* ctx);

// Free any associated resources, and finish writing if necessary
void queltdb_close(QueltDB* db);


#endif

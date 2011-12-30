// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <zlib.h>
#include "database.h"
#include "quelt-common.h"

// Compatibility shim for Windows
#ifdef _WIN32
# define ftello _ftelli64
#endif

struct QueltDB {
	// Indicates whether this database is opened for 'w'riting or 'r'eading
	char open_mode;
	int32_t n_articles;
	// The offset in the database file where the current article started
	f_offset article_start;

	bool in_article;
	z_stream compression_ctx;
	FILE* indexfile;
	FILE* dbfile;
};

static QueltDB* _queltdb_new(void) {
	QueltDB* db = malloc(sizeof(QueltDB));
	db->open_mode = 0;
	db->n_articles = 0;
	db->article_start = 0;
	db->in_article = false;

	db->indexfile = NULL;
	db->dbfile = NULL;

	return db;
}

QueltDB* queltdb_create(void) {
	QueltDB* db = _queltdb_new();
	db->open_mode = 'w';

	// Prepare our compression stream
	db->compression_ctx.zalloc = Z_NULL;
	db->compression_ctx.zfree = Z_NULL;
	db->compression_ctx.opaque = Z_NULL;

	db->indexfile = fopen("quelt.index", "wb");
	fwrite(&db->n_articles, sizeof(int32_t), 1, db->indexfile);

	db->dbfile = fopen("quelt.db", "wb");

	return db;
}

static void _write_chunk(QueltDB* db, const char* s, int len, int flush) {
	const size_t chunk_len = 2048;
	Bytef buf[chunk_len];
	db->compression_ctx.next_in = (Bytef*)s;
	db->compression_ctx.avail_in = len;

	// Write until zlib's output buffer is empty
	do {
		db->compression_ctx.avail_out = chunk_len;
		db->compression_ctx.next_out = buf;
		deflate(&db->compression_ctx, flush);

		const size_t remaining = chunk_len - db->compression_ctx.avail_out;
		fwrite(buf, sizeof(Bytef), remaining, db->dbfile);
	} while(db->compression_ctx.avail_out == 0);
}

void queltdb_writechunk(QueltDB* db, const char* buf, size_t len) {
	if(!db->in_article) {
		deflateInit(&db->compression_ctx, Z_BEST_COMPRESSION);
		db->in_article = true;
		db->article_start = ftello(db->dbfile);
	}

	_write_chunk(db, buf, len, Z_NO_FLUSH);
}

void queltdb_finisharticle(QueltDB* db, const char* title, size_t len) {
	// Finish the compression stream
	_write_chunk(db, NULL, 0, Z_FINISH);
	deflateEnd(&db->compression_ctx);

	// The title we're given might be shorter than MAX_TITLE_LEN.  Pad it out.
	char buf[MAX_TITLE_LEN] = {0};
	memcpy(buf, title, len);

	// Write the index record
	fwrite(buf, sizeof(char), MAX_TITLE_LEN, db->indexfile);
	fwrite(&(db->article_start), sizeof(f_offset), 1, db->indexfile);

	db->in_article = false;
	db->n_articles += 1;
}

QueltDB* queltdb_open(void) {
	QueltDB* db = _queltdb_new();
	db->open_mode = 'r';
	
	db->indexfile = fopen("quelt.index", "rb");
	fread(&db->n_articles, sizeof(int32_t), 1, db->indexfile);

	db->dbfile = fopen("quelt.db", "rb");

	return db;
}

int queltdb_narticles(const QueltDB* db) {
	return db->n_articles;
}

void queltdb_search(QueltDB* db, const char* needle,
					queltdb_handler_func handler, void* ctx) {
	char title[MAX_TITLE_LEN+1];
	f_offset start = 0;

	// For each field, check to see if needle is in that title.  If so, call
	// the provided handler.
	while(!feof(db->indexfile)) {
		fread(title, sizeof(char), MAX_TITLE_LEN, db->indexfile);
		fread(&start, sizeof(f_offset), 1, db->indexfile);
		if(strstr(title, needle) != NULL) {
			handler(ctx, title, MAX_TITLE_LEN);
		}
	}
}

void queltdb_getarticle(QueltDB* db, const char* title,
						queltdb_handler_func* handler, void* ctx) {
	// STUB
}

void queltdb_close(QueltDB* db) {
	if(!db) return;

	if(db->open_mode == 'w') {
		fseek(db->indexfile, 0, SEEK_SET);
		fwrite(&db->n_articles, sizeof(int32_t), 1, db->indexfile);
	}

	fclose(db->indexfile);
	fclose(db->dbfile);
	free(db);
}

// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#define _LARGEFILE_SOURCE

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <zlib.h>
#include "pprint.h"
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
    if(!db) {
        return NULL;
    }

    db->open_mode = 'w';

    // Prepare our compression stream
    db->compression_ctx.zalloc = Z_NULL;
    db->compression_ctx.zfree = Z_NULL;
    db->compression_ctx.opaque = Z_NULL;

    db->indexfile = fopen("quelt.index", "wb");
    db->dbfile = fopen("quelt.db", "wb");

    if(!db->indexfile || !db->dbfile) {
        free(db);
        return NULL;
    }

    // Pad out a header for an article count
    fwrite(&db->n_articles, sizeof(int32_t), 1, db->indexfile);

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

static void _queltdb_sendarticle(QueltDB* db, f_offset offset,
                                queltdb_handler_func handler, void* ctx) {
    int status = Z_OK;
    z_stream decompression_ctx;
    decompression_ctx.zalloc = Z_NULL;
    decompression_ctx.zfree = Z_NULL;
    decompression_ctx.opaque = Z_NULL;
    decompression_ctx.avail_in = 0;
    decompression_ctx.next_in = Z_NULL;
    inflateInit(&decompression_ctx);

    const int chunk_len = 2048;
    Bytef in[chunk_len];
    Bytef out[chunk_len];

    fseeko(db->dbfile, offset, SEEK_SET);

    // Read chunks until the stream ends
    do {
        decompression_ctx.avail_in = fread(in, sizeof(char), chunk_len, db->dbfile);
        if (decompression_ctx.avail_in == 0)
            break;
        decompression_ctx.next_in = in;

        // Inflate until the zlib buffer is empty
        do {
            decompression_ctx.avail_out = chunk_len;
            decompression_ctx.next_out = out;
            status = inflate(&decompression_ctx, Z_NO_FLUSH);
            const int remaining = chunk_len - decompression_ctx.avail_out;
            handler(ctx, (char*)out, remaining);
        } while (decompression_ctx.avail_out == 0);
    } while (status != Z_STREAM_END);

    inflateEnd(&decompression_ctx);
}

void queltdb_getarticle(QueltDB* db, const char* article,
                        queltdb_handler_func handler, void* ctx) {
    char title[MAX_TITLE_LEN+1];
    f_offset start = 0;

    // Skip past the index header
    fseeko(db->indexfile, sizeof(int32_t), SEEK_SET);

    while(!feof(db->indexfile)) {
        fread(title, sizeof(char), MAX_TITLE_LEN, db->indexfile);
        fread(&start, sizeof(f_offset), 1, db->indexfile);
        if(strcmp(title, article) == 0) {
            _queltdb_sendarticle(db, start, handler, ctx);

            // We have what we want.  Short-circuit
            return;
        }
    }
}

// XXX Partial binary search implementation of the above.  I thought Wikipedia
// dumps were sorted.  They're not.  We need to take care of that first.
/*
// Find a midpoint, avoiding the low+high<0 overflow problem.
static inline int32_t _midpoint(int32_t low, int32_t high) {
    // Cast to unsigned necessary to get a logical rshift
    return ((uint32_t)low + (uint32_t)high) >> 1;
}

void queltdb_getarticle(QueltDB* db, const char* title,
                        queltdb_handler_func* handler, void* ctx) {
    char cur_title[MAX_TITLE_LEN+1];
    f_offset start = 0;
    const size_t index_start = sizeof(int32_t);
    int32_t low = 0;
    int32_t high = db->n_articles - 1;
    int32_t cur = _midpoint(low, high);

    // Use a sanity guard to make sure a bug doesn't send our algorithm
    // spinning off forever.  XXX: Do we *really* need this?
    const float worst_case = ceilf(log2(db->n_articles));
    float comparisons = 0;

    while(comparisons <= worst_case) {
        fseeko(db->indexfile, (index_start + cur*(MAX_TITLE_LEN+sizeof(f_offset))), SEEK_SET);
        fread(cur_title, sizeof(char), MAX_TITLE_LEN, db->indexfile);
        fread(&start, sizeof(f_offset), 1, db->indexfile);
        log_printf("%d %s", cur, cur_title);
        const int cmp = strcmp(title, cur_title);
        if(cmp < 0) {
            high = cur - 1;
        }
        else if(cmp > 0) {
            low = cur + 1;
        }
        else {
            log("Found!");
            break;
        }

        cur = _midpoint(low, high);
        comparisons += 1;
    }
    log("Failed");
}
*/

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

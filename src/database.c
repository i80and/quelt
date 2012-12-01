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

#define HEADER_LEN (sizeof(int32_t)+sizeof(int32_t))
#define RECORD_LEN (255+sizeof(f_offset))

struct QueltDB {
    // Indicates whether this database is opened for 'w'riting or 'r'eading
    char open_mode;
    int32_t n_articles;
    int32_t segment_length;
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

static void _queltdb_free(QueltDB* db) {
    free(db);
}

QueltDB* queltdb_create(int32_t segment_length) {
    QueltDB* db = _queltdb_new();
    if(!db) {
        return NULL;
    }

    db->open_mode = 'w';

    // Prepare our compression stream
    db->compression_ctx.zalloc = Z_NULL;
    db->compression_ctx.zfree = Z_NULL;
    db->compression_ctx.opaque = Z_NULL;

    db->indexfile = fopen("quelt.index", "wb+");
    db->dbfile = fopen("quelt.db", "wb+");

    if(!db->indexfile || !db->dbfile) {
        _queltdb_free(db);
        return NULL;
    }

    // Pad out a header for an article count
    fwrite(&db->n_articles, sizeof(int32_t), 1, db->indexfile);

    // Pad out a header for a segment length
    db->segment_length = segment_length;
    fwrite(&db->segment_length, sizeof(int32_t), 1, db->indexfile);

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

    // Try to open our database files
    db->indexfile = fopen("quelt.index", "rb");
    db->dbfile = fopen("quelt.db", "rb");
    if(!db->dbfile || !db->indexfile) {
        if(db->dbfile) fclose(db->dbfile);
        if(db->indexfile) fclose(db->indexfile);
        _queltdb_free(db);
        return NULL;
    }

    // Read the index header
    fread(&db->n_articles, sizeof(int32_t), 1, db->indexfile);
    fread(&db->segment_length, sizeof(int32_t), 1, db->indexfile);

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

int queltdb_getarticle_linear(QueltDB* db, const char* article,
                        queltdb_handler_func handler, void* ctx) {
    char title[MAX_TITLE_LEN+1];
    f_offset start = 0;

    // Skip past the index header
    fseeko(db->indexfile, HEADER_LEN, SEEK_SET);

    while(!feof(db->indexfile)) {
        fread(title, sizeof(char), MAX_TITLE_LEN, db->indexfile);
        fread(&start, sizeof(f_offset), 1, db->indexfile);
        if(strcmp(title, article) == 0) {
            _queltdb_sendarticle(db, start, handler, ctx);

            // We have what we want.  Short-circuit
            return 1;
        }
    }

    return 0;
}

// XXX Partial binary search implementation of the above.  I thought Wikipedia
// dumps were sorted.  They're not.  We need to take care of that first.

// Find a midpoint, avoiding the low+high<0 overflow problem.
static inline int32_t midpoint(int32_t low, int32_t high) {
    // Cast to unsigned necessary to get a logical rshift
    return ((uint32_t)low + (uint32_t)high) >> 1;
}

static int32_t queltdb_search_segment(QueltDB* db,
                                      const char* title,
                                      int32_t segment) {
    char cur_title[MAX_TITLE_LEN+1];
    f_offset index_start = HEADER_LEN + (segment * db->segment_length * RECORD_LEN);

    int32_t low = 0;
    int32_t high = db->segment_length - 1;
    int32_t old_cur = -1;
    int32_t cur = midpoint(low, high);

    while(old_cur != cur) {
        fseeko(db->indexfile, (index_start + cur*RECORD_LEN), SEEK_SET);
        fread(cur_title, sizeof(char), MAX_TITLE_LEN, db->indexfile);

        const int cmp = strcmp(title, cur_title);
        if(cmp < 0) {
            high = cur - 1;
        }
        else if(cmp > 0) {
            low = cur + 1;
        }
        else {
            return segment + cur;
        }

        old_cur = cur;
        cur = midpoint(low, high);
    }

    return -1;
}

int queltdb_getarticle(QueltDB* db, const char* title,
                       queltdb_handler_func handler, void* ctx) {
    const int32_t n_segments = db->n_articles / db->segment_length + \
        ((db->n_articles % db->segment_length == 0)? 0 : 1);

    for(int32_t i = 0; i < n_segments; i += 1) {
        int32_t rec_no = queltdb_search_segment(db, title, i);
        if(rec_no >= 0) {
            f_offset start = 0;
            fseeko(db->indexfile, HEADER_LEN+(rec_no*RECORD_LEN)+MAX_TITLE_LEN, SEEK_SET);
            fread(&start, sizeof(start), 1, db->indexfile);
            _queltdb_sendarticle(db, start, handler, ctx);
            return 1;
        }
    }

    return 0;
}

static int record_cmp(const void* rec1, const void* rec2) {
    char rec1_title[MAX_TITLE_LEN+1];
    char rec2_title[MAX_TITLE_LEN+1];
    memcpy(rec1_title, rec1, MAX_TITLE_LEN);
    rec1_title[MAX_TITLE_LEN] = '\0';
    memcpy(rec2_title, rec2, MAX_TITLE_LEN);
    rec2_title[MAX_TITLE_LEN] = '\0';

    return strcmp(rec1_title, rec2_title);
}

// Sort our index for quick searching
static void queltdb_sort_index(QueltDB* db) {
    fseek(db->indexfile, HEADER_LEN, SEEK_SET);
    void* buf = malloc(RECORD_LEN*db->segment_length);

    int32_t i = 0;
    while(db->n_articles >= i) {
        const int32_t chunk_len = (db->n_articles >= (i + db->segment_length))?
               db->segment_length : (db->n_articles - i);
        fread(buf, RECORD_LEN, chunk_len, db->indexfile);

        qsort(buf, chunk_len, RECORD_LEN, &record_cmp);

        // Rewind to start of segment
        fseek(db->indexfile, -chunk_len*RECORD_LEN, SEEK_CUR);
        fwrite(buf, RECORD_LEN, chunk_len, db->indexfile);

        i += db->segment_length;
    }

    free(buf);
}

void queltdb_close(QueltDB* db) {
    if(!db) return;

    if(db->open_mode == 'w') {
        fseek(db->indexfile, 0, SEEK_SET);
        fwrite(&db->n_articles, sizeof(int32_t), 1, db->indexfile);

        // Only write out our segment length if it's still unknown.
        if(db->segment_length == 0) {
            db->segment_length = db->n_articles;
            fwrite(&db->segment_length, sizeof(db->segment_length), 1, db->indexfile);
        }

        queltdb_sort_index(db);
    }

    fclose(db->indexfile);
    fclose(db->dbfile);
    _queltdb_free(db);
}

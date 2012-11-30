// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <expat.h>
#include <zlib.h>
#include "database.h"
#include "pprint.h"
#include "quelt-common.h"

// A reasonable default
#define SEGMENT_LENGTH 10000

// The command line option -v gives additional information during processing
static bool option_verbose = false;

// The command line option --noredirects disables saving articles that start
// with a redirect directive.
static bool option_noredirects = false;

// Our return code is a bitfield.  Don't rely on these to not change just yet
#define RETURN_BADXML 4
#define RETURN_WRITEERROR 8
#define RETURN_INTERNALERROR 128

typedef enum {
    LOCATION_NULL,
    LOCATION_TITLE,
    LOCATION_ARTICLE_START,
    LOCATION_TEXT
} ParseLocation;

typedef struct {
    QueltDB* db;
    // 255 is the maximum length of a Wikipedia article title, plus one for \0
    char title[MAX_TITLE_LEN];
    short title_cursor;
    // Current parse context
    ParseLocation location;
} ParseCtx;

void parsectx_init(ParseCtx* ctx, const char* dbpath, const char* indexpath) {
    memset(ctx, 0, sizeof(ParseCtx));
    ctx->db = queltdb_create(SEGMENT_LENGTH);
    if(!ctx->db) {
        fail(RETURN_INTERNALERROR, "Could not open database");
    }
}

void handle_starttag(ParseCtx* ctx, const XML_Char* tag, const XML_Char** attrs) {
    if(strcmp(tag, "text") == 0) {
        ctx->location = LOCATION_ARTICLE_START;
    }
    else if(strcmp(tag, "title") == 0) {
        ctx->location = LOCATION_TITLE;
        memset(ctx->title, 0, MAX_TITLE_LEN);
        ctx->title_cursor = 0;
    }
}

void handle_endtag(ParseCtx* ctx, const XML_Char* tag) {
    if((strcmp(tag, "text") == 0) && (ctx->location == LOCATION_TEXT)) {
        // Write this article into the db
        queltdb_finisharticle(ctx->db, ctx->title, MAX_TITLE_LEN);
        ctx->location = LOCATION_NULL;
    }
    else if(strcmp(tag, "title") == 0) {
        ctx->location = LOCATION_NULL;
        if(option_verbose) printf("Processing %s\n", ctx->title);
    }
}

// Return whether the first len bytes of body indicate a redirect page
static inline bool is_redirect(const char* body, int len) {
    static const char* REDIRECT_TOKEN = "#REDIRECT";
    static const int REDIRECT_TOKEN_LEN = 9;

    const int min = (len < REDIRECT_TOKEN_LEN)? len : REDIRECT_TOKEN_LEN;

    // If body is shorter than #REDIRECT, that causes an ugly corner-case where
    // # is compared to the first byte of #REDIRECT.  Which is a false-positive
    if(len >= REDIRECT_TOKEN_LEN && strncmp(body, REDIRECT_TOKEN, min) == 0) {
        return true;
    }

    return false;
}

void handle_chardata(ParseCtx* ctx, const XML_Char* s, int len) {
    if(ctx->location == LOCATION_ARTICLE_START) {
        // If instructed, skip this article if it seems to be a redirect
        if(option_noredirects && is_redirect(s, len)) {
            ctx->location = LOCATION_NULL;
            if(option_verbose) printf("Skipping %s\n", ctx->title);
            return;
        }

        ctx->location = LOCATION_TEXT;
    }

    if(ctx->location == LOCATION_TEXT) {
        queltdb_writechunk(ctx->db, s, len*sizeof(XML_Char));
    }
    else if(ctx->location == LOCATION_TITLE) {
        // Multiple calls may be required to finish this title, and it is
        // hypothetically possible for us to overrun our buffer without this.
        const size_t n_safe_bytes = (len + ctx->title_cursor >= MAX_TITLE_LEN)?
            MAX_TITLE_LEN - ctx->title_cursor:
            len;
        memcpy(ctx->title + ctx->title_cursor, s, n_safe_bytes);
        ctx->title_cursor += n_safe_bytes;
    }
}

void parse(const char* path) {
    ParseCtx ctx;
    parsectx_init(&ctx, "quelt.db", "quelt.index");

    // Initialize the XML parser
    XML_Parser parser = XML_ParserCreate("UTF-8");
    XML_SetElementHandler(parser,
        (XML_StartElementHandler)&handle_starttag,
        (XML_EndElementHandler)&handle_endtag);
    XML_SetCharacterDataHandler(parser,
        (XML_CharacterDataHandler)&handle_chardata);
    XML_SetUserData(parser, &ctx);

    FILE* infile = fopen(path, "r");
    if(!infile) {
        fprintf(stderr, "Could not open %s\n", path);
        fail(RETURN_BADFILE, NULL);
    }

    // TODO: Choose buffer based on filesystem block size
    const size_t buffer_len = 1024;
    char buffer[buffer_len];

    // Read chunks from the file and feed them into the XML parser until EOF
    bool done = false;
    while(!done) {
        size_t n_bytes = fread(buffer, sizeof(XML_Char), buffer_len, infile);
        if(n_bytes < buffer_len) {
            done = true;
        }

        if(!XML_Parse(parser, buffer, n_bytes, done)) {
            set_return_flag(RETURN_BADXML);
            fail(RETURN_BADXML, "Invalid XML");
        }
    }

    printf("Sorting\n");
    queltdb_close(ctx.db);
    fclose(infile);
    XML_ParserFree(parser);
}

static void parse_argument(const char* arg) {
    if(strcmp(arg, "-v") == 0) {
        option_verbose = true;
    }
    else if(strcmp(arg, "--noredirects") == 0) {
        option_noredirects = true;
    }
    else {
        fail(RETURN_BADARGS, "Unrecognized argument");
    }
}

int main(int argc, char** argv) {
    if(argc <= 1) {
        log("No XML dump specified.\n"
            "Usage: quelt-split db [-v] [--noredirects]");
        return RETURN_BADARGS;
    }

    for(int i = 2; i < argc; i+=1) {
        parse_argument(argv[i]);
    }

    const char* path = argv[1];
    parse(path);

    return RETURN_OK;
}

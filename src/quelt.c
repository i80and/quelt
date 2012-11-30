// Copyright (c) 2011 Andrew Aldridge under the terms in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <string.h>
#include "database.h"
#include "quelt-common.h"
#include "pprint.h"

static bool option_search = false;
static bool option_plain = false;

#define RETURN_NOMATCH 3
#define RETURN_UNKNOWNERROR 128

typedef struct {
    int template_depth;
    char prev_byte;
} HandlerCtx;

void search_match_handler(void* ctx, char* title, size_t title_len) {
    printf("%s\n", title);
}

void raw_chunk_handler(void* ctx, char* data, size_t chunk_len) {
    fwrite(data, chunk_len, 1, stdout);
}

void fancy_chunk_handler(void* rawctx, char* data, size_t chunk_len) {
    // For now, we just completely remove all templates.  Parsing WikiMarkup
    // in any meaningful way is a bottomless rabbit hole of death and
    // corner-cases
    HandlerCtx* ctx = (HandlerCtx*)rawctx;
    size_t i = 0;

    while(i < chunk_len) {
        if(i == chunk_len - 1) {
            // This is the last byte, so we can't tell if it's starting a token
            // XXX FINISH ME
            if(data[i] == '{' || data[i] == '}') {
                ctx->prev_byte = data[i];
            }
            else {
                ctx->prev_byte = 0;
            }

            return;
        }

        if(data[i] == '{' && data[i+1] == '{') {
            ctx->template_depth += 1;
            i += 2;
        }
        else if(data[i] == '}' && data[i+1] == '}') {
            ctx->template_depth -= 1;
            i += 2;

            // If this is our last nested template, add a skipped content marker
            if(ctx->template_depth == 0) fputs("###", stdout);
        }
        else {
            if(ctx->template_depth == 0) {
                fputc(data[i], stdout);
            }

            i += 1;
        }
    }
}

// Perform a linear-time search 
static void search(QueltDB* db, const char* title) {
    queltdb_search(db, title, &search_match_handler, NULL);
}

void parse_argument(const char* arg) {
    if(!option_search && strcmp(arg, "--search") == 0) {
        option_search = true;
    }
    else if(!option_plain && strcmp(arg, "--plain") == 0) {
        option_plain = true;
    }
    else {
        fail(RETURN_BADARGS, "Unrecognized argument");
    }
}

int main(int argc, char** argv) {
    if(argc < 2) {
        log("No article specified\nUsage: quelt article [--search] [--plain]");
        return RETURN_BADARGS;
    }

    const char* article = argv[1];

    for(int i = 2; i < argc; i+=1) {
        parse_argument(argv[i]);
    }

    QueltDB* db = queltdb_open();
    if(!db) {
        log("Could not open database.");
        return RETURN_BADFILE;
    }

    int found = 0;
    if(option_search) {
        search(db, article);
    }
    else {
        if(option_plain) {
            found = queltdb_getarticle(db, article, &raw_chunk_handler, NULL);
        }
        else {
            HandlerCtx ctx = {0, 0};
            found = queltdb_getarticle(db, article, &fancy_chunk_handler, &ctx);
        }
    }

    queltdb_close(db);

    if(!found) return RETURN_NOMATCH;

    return 0;
}

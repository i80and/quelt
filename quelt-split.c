#define _XOPEN_SOURCE 600

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <expat.h>
#include <zlib.h>
#include "pprint.h"

// Technically this should be off_t, but we always want it to be 64-bit
typedef int64_t f_offset;
// Compatibility shim for Windows
#ifdef _WIN32
# define ftello _ftelli64
#endif

// The command-line option -v sets this to true
static bool option_verbose = false;

// Our return code is a bitfield
#define RETURN_OK 0
#define RETURN_BADARGS 1
#define RETURN_BADXML 2
#define RETURN_WRITEERROR 4
#define RETURN_IMPROPERLYSORTED 8
#define RETURN_INTERNALERROR 16
static int return_status = RETURN_OK;

// Add a flag to our return code
void set_return_flag(int flag) {
	if(!(return_status & flag)) {
		return_status |= flag;
	}
}

// Early exit with our return flags
void fail(void) {
	exit(return_status);
}

typedef enum {
	LOCATION_NULL,
	LOCATION_TITLE,
	LOCATION_TIMESTAMP,
	LOCATION_TEXT
} ParseLocation;

#define MAX_TITLE_LEN 255
typedef struct {
	// 255 is the maximum length of a Wikipedia article title, plus one for \0
	char title[256];
	size_t title_cursor;
	// Start of the current article in the output file.  This *technically*
	f_offset article_start;
	//Compression buffer and stream
	z_stream compression_ctx;
	// Output streams
	FILE* dbfile;
	FILE* indexfile;
	// Current parse context
	ParseLocation location;
} ParseCtx;

static void parsectx_init(ParseCtx* ctx, const char* dbpath, const char* indexpath) {
	memset(ctx, 0, sizeof(ParseCtx));
	
	ctx->dbfile = fopen(dbpath, "wb");
	ctx->indexfile = fopen(indexpath, "wb");
	ctx->compression_ctx.zalloc = Z_NULL;
	ctx->compression_ctx.zfree = Z_NULL;
	ctx->compression_ctx.opaque = Z_NULL;
}

// Compress and write a chunk of an article to the database
static void write_chunk(ParseCtx* ctx, const XML_Char* s, int len, int flush) {
	const size_t chunk_len = 2048;
	Bytef buf[chunk_len];
	ctx->compression_ctx.next_in = (Bytef*)s;
	ctx->compression_ctx.avail_in = len;

	// Write until zlib's output buffer is empty
	do {
		ctx->compression_ctx.avail_out = chunk_len;
		ctx->compression_ctx.next_out = buf;
		deflate(&ctx->compression_ctx, flush);

		const size_t remaining = chunk_len - ctx->compression_ctx.avail_out;
		fwrite(buf, sizeof(Bytef), remaining, ctx->dbfile);
		if(ferror(ctx->dbfile)) {
			log("Write error");
			set_return_flag(RETURN_WRITEERROR);
			fail();
		}
	} while(ctx->compression_ctx.avail_out == 0);
}

static void handle_starttag(ParseCtx* ctx, const XML_Char* tag, const XML_Char** attrs) {
	if(strcmp(tag, "text") == 0) {
		ctx->location = LOCATION_TEXT;
		ctx->article_start = ftello(ctx->dbfile);
		
		if(deflateInit(&ctx->compression_ctx, Z_BEST_COMPRESSION) != Z_OK) {
			log("Error initializing zlib");
			set_return_flag(RETURN_INTERNALERROR);
			fail();
		}
	}
	else if(strcmp(tag, "title") == 0) {
		ctx->location = LOCATION_TITLE;
		ctx->title_cursor = 0;
	}
}

static void handle_endtag(ParseCtx* ctx, const XML_Char* tag) {
	if(strcmp(tag, "text") == 0) {
		// Write this article into the db
		fwrite(ctx->title, sizeof(char), MAX_TITLE_LEN, ctx->indexfile);
		fwrite(&(ctx->article_start), sizeof(f_offset), 1, ctx->indexfile);
		ctx->location = LOCATION_NULL;
		
		// Finish and close the current zlib stream
		write_chunk(ctx, NULL, 0, Z_FINISH);
		deflateEnd(&ctx->compression_ctx);
	}
	else if(strcmp(tag, "title") == 0) {
		ctx->location = LOCATION_NULL;
		if(option_verbose) {log_printf("Processing %s", ctx->title);}

		// Ensure that our title is zero-terminated
		ctx->title[ctx->title_cursor] = '\0';
	}
}

static void handle_chardata(ParseCtx* ctx, const XML_Char* s, int len) {
	if(ctx->location == LOCATION_TEXT) {
		write_chunk(ctx, s, len, Z_NO_FLUSH);
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
			fail();
		}
	}
	
	fclose(infile);
	fclose(ctx.dbfile);
	fclose(ctx.indexfile);
	XML_ParserFree(parser);
}

int main(int argc, char** argv) {
	// quelt-split db [-v]
	if(argc <= 1) {
		log("No XML dump specified.\nUsage: quelt-split db [-v]");
		return RETURN_BADARGS;
	}

	// Check for verbose option
	if(argc >= 3 && !strcmp(argv[2], "-v")) {
		option_verbose = true;
	}

	const char* path = argv[1];
	parse(path);

	return RETURN_OK;
}

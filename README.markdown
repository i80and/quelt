Quelt
=====
A lightweight offline Wikipedia reader.

File format
-----------
The initial plan was for Quelt to use a separate file for every article, using
the path for the article name.  A cute idea, but reality set in fairly quickly:
    * Filename restrictions, especially on NTFS
    * Standard filesystem tools are not built to handle 3 million+ files easily
So instead, a custom binary format is used with two files: quelt.db, and
quelt.index.

quelt.db is just concatenated zlib streams, each containing an article.
Ordering is defined by the input, which should be pre-sorted byte-wise.

quelt.index may be described as a simple array of structs, sharing the same
order as quelt.db.
struct Field {
	char[255] title;
	int64_t article_start;
};

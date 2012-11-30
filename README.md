Quelt
=====
A lightweight offline Wikipedia reader.

Compilation Requirements
------------
* C99 compiler
* Unix environment.  Some win32 shims exist, but they are untested.
* Expat (only for quelt-split)
* Zlib

Building
--------
    $ make

Usage
-----
    $ ./quelt-split [path to XML dump] [-v]
    $ ./quelt [part of title] --search [--plain]
    $ ./quelt [exact title] [--plain]

File format
-----------
The initial plan was for Quelt to use a separate file for every article, using
the path for the article name.  A cute idea, but reality set in fairly quickly:

* Filename restrictions, especially on NTFS
* Standard filesystem tools are not built to handle 3 million+ files easily

So instead, a custom binary format is used with two files: `quelt.db`, and
`quelt.index`.

`quelt.index`:

    | n_articles:       Int32
    | segment_length:   Int32
    | article 0 title:  Byte[255]
    | article 0 offset: Int64
    | article 1 title:  Byte[255]
    | article 1 offset: Int64
    | article n title:  Byte[255]
    | article n offset: Int64

`quelt.db` is a concatenated sequence of zlib streams, where the start of each
article is given by the article offsets in `quelt.index`

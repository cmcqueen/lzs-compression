
bin_PROGRAMS = lzs-compress lzs-decompress

AM_CFLAGS = -I$(srcdir)/../liblzs

lzs_compress_SOURCES = lzs-compress.c
lzs_compress_LDADD = ../liblzs/lib@PACKAGE_NAME@.la

lzs_decompress_SOURCES = lzs-decompress.c
lzs_decompress_LDADD = ../liblzs/lib@PACKAGE_NAME@.la

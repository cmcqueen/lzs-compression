
#######################################
# Tests

bin_PROGRAMS = test-incremental

TESTS = test-lzs-decompression test-incremental

check_PROGRAMS = test-lzs-decompression test-incremental

AM_CFLAGS = -I$(srcdir)/../liblzs

test_lzs_decompression_SOURCES = test-lzs-decompression.c
test_lzs_decompression_LDADD = ../liblzs/lib@PACKAGE_NAME@.la

test_incremental_SOURCES = test-incremental.c
test_incremental_LDADD = ../liblzs/lib@PACKAGE_NAME@.la

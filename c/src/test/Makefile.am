
#######################################
# Tests

TESTS = test-lzs test-lzs-decompression

check_PROGRAMS = test-lzs test-lzs-decompression

AM_CFLAGS = -I$(srcdir)/../liblzs -I$(srcdir)/unity

test_lzs_SOURCES = test-lzs.c unity/unity.c
test_lzs_LDADD = ../liblzs/lib@PACKAGE_NAME@.la

test_lzs_decompression_SOURCES = test-lzs-decompression.c unity/unity.c
test_lzs_decompression_LDADD = ../liblzs/lib@PACKAGE_NAME@.la


#######################################
# Tests

TESTS = test-lzs-decompression

check_PROGRAMS = test-lzs-decompression

AM_CFLAGS = -I$(srcdir)/../liblzs

test_lzs_decompression_CFLAGS = -I$(srcdir)/../liblzs -I$(srcdir)/unity
test_lzs_decompression_SOURCES = test-lzs-decompression.c unity/unity.c
test_lzs_decompression_LDADD = ../liblzs/lib@PACKAGE_NAME@.la

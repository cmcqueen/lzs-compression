
#######################################
# Tests

TESTS = test-lzs-decompression

check_PROGRAMS = test-lzs-decompression

AM_CFLAGS = -I$(srcdir)/../liblzs

test_lzs_decompression_SOURCES = test-lzs-decompression.c
test_lzs_decompression_LDADD = ../liblzs/lib@PACKAGE_NAME@-@PACKAGE_VERSION@.la

/*****************************************************************************
 *
 * \file test-lzs-decompression.c
 *
 * \brief Unit Tests for Embedded Compression and Decompression
 *
 ****************************************************************************/


/*****************************************************************************
 * Includes
 ****************************************************************************/

#include "lzs.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>         /* For memset() */


/*****************************************************************************
 * Defines
 ****************************************************************************/

#define OUT_BUFFER_EXTRA_LEN    520u
#define IN_BUFFER_BOUNDED_LEN   10u
#define OUT_BUFFER_BOUNDED_LEN  10u


/*****************************************************************************
 * Tables
 ****************************************************************************/

static const uint8_t compressed_data_1[] =
{
    0x29, 0x19, 0x4E, 0x87, 0x53, 0x91, 0xB8, 0x40,
    0x61, 0x10, 0x1C, 0xCE, 0x87, 0x23, 0x49, 0xB8,
    0xCE, 0x20, 0x31, 0x9B, 0xCD, 0xC7, 0x43, 0x0E,
    0x24, 0xC5, 0xD9, 0x40, 0xE1, 0x93, 0x71, 0xC1,
    0x88, 0xD8, 0x65, 0x10, 0x1C, 0x8C, 0xB8, 0xC0,
    0xCA, 0x73, 0x32, 0xE3, 0x93, 0xA1, 0xA7, 0x44,
    0x08, 0x0D, 0xE6, 0x6C, 0xF1, 0x86, 0x4C, 0x46,
    0xA3, 0x29, 0x8C, 0xE8, 0x2E, 0x10, 0x11, 0x8D,
    0xE7, 0x21, 0x01, 0xB7, 0x20, 0x1E, 0x44, 0x07,
    0x43, 0xC9, 0xC3, 0x4A, 0x0B, 0x30, 0xE1, 0xA0,
    0xD2, 0x73, 0x10, 0x19, 0x8E, 0xA6, 0xEC, 0xE1,
    0xAC, 0xB3, 0x41, 0xAF, 0x2A, 0x6B, 0xED, 0xD8,
    0x74, 0x32, 0x9B, 0x4E, 0x07, 0x4C, 0xF8, 0x6F,
    0xE7, 0xBF, 0x6F, 0x6D, 0x91, 0x9D, 0x04, 0x07,
    0x73, 0x79, 0xD4, 0xD8, 0x64, 0x10, 0x1E, 0x4D,
    0x26, 0x5C, 0x33, 0xDE, 0xF1, 0xCB, 0x06, 0x9C,
    0xE9, 0xA0, 0x61, 0x06, 0x1C, 0xCC, 0x26, 0xDC,
    0x28, 0x76, 0x30, 0x9B, 0x0E, 0xB8, 0x60, 0xEF,
    0x8F, 0x34, 0x01, 0xC0, 0xC2, 0x73, 0x84, 0x89,
    0xAA, 0x39, 0xC9, 0x97, 0x2C, 0x8A, 0x05, 0x3F,
    0x80, 0xDF, 0xA7, 0x4E, 0x5B, 0x03, 0x2C, 0x6C,
    0x32, 0x16, 0x7E, 0xF0, 0x93, 0x7D, 0xFE, 0x39,
    0x50, 0xC6, 0x6C, 0x37, 0xEF, 0x6C, 0x69, 0xEA,
    0x4C, 0xF0, 0xE0, 0x4C, 0x47, 0x23, 0x09, 0x8E,
    0x14, 0xE1, 0xD3, 0x48, 0x42, 0x2B, 0x22, 0x0B,
    0xD8, 0xEB, 0x36, 0x1B, 0xBF, 0x14, 0x3C, 0x5C,
    0x65, 0x0E, 0x1B, 0x19, 0xE1, 0x0A, 0x0E, 0x84,
    0x00, 0x67, 0xDC, 0x1F, 0x86, 0x15, 0x3A, 0x61,
    0x32, 0x19, 0x30, 0xE7, 0xBD, 0x84, 0x89, 0xDC,
    0x4C, 0xD1, 0x18, 0x21, 0xFA, 0x44, 0xC3, 0xA1,
    0xF6, 0x42, 0x0D, 0xFB, 0xA7, 0x5D, 0x01, 0x0F,
#if 1
    0x0C, 0xEC, 0x77, 0xF8, 0x87, 0x63, 0xB4, 0x21,
    0x52, 0xFB, 0x4E, 0xC7, 0xAC, 0x55, 0x82, 0x0C,
    0x21, 0xC3, 0x64, 0x38, 0xDC, 0x31, 0x10, 0xC6,
#if 1
    0x12, 0x31, 0xC8, 0xDF, 0xD4, 0x21, 0xF2, 0x42,
    0x79, 0x8A, 0xFF, 0xB4, 0x52, 0xF7, 0x22, 0xC6,
#if 1
    0x15, 0xFD, 0x98, 0x82, 0x16, 0xEC, 0x2F, 0x0C,
    0xE1, 0x17, 0x34, 0xC5, 0x19, 0x10, 0xC8, 0x65,
    0x33, 0x47, 0x33, 0xC0, 0xBE, 0x5F, 0x88, 0x8D,
    0x86, 0x22, 0x52, 0x46, 0x48, 0x85, 0x8A, 0x6F,
    0x32, 0x0B, 0xB0, 0x00,
#if 0
    0x29, 0x19, 0x4E, 0x87, 0x53, 0x91, 0xB8, 0x40,
    0x61, 0x10, 0x1C, 0xCE, 0x87, 0x23, 0x49, 0xB8,
    0xCE, 0x20, 0x31, 0x9B, 0xCD, 0xC7, 0x43, 0x0E,
#endif
#endif
#endif
#endif
};

static const uint8_t decompressed_data_1[] =
    "Return a string containing a printable representation of an object. For many types, this "
    "function makes an attempt to return a string that would yield an object with the same value "
    "when passed to eval(), otherwise the representation is a string enclosed in angle brackets "
    "that contains the name of the type of the object together with additional information often "
    "including the name and address of the object. A class can control what this function returns "
    "for its instances by defining a __repr__() method.";


/*****************************************************************************
 * Test functions
 ****************************************************************************/

/**
 * \brief Test for lzs_decompress()
 */
static void test_lzs_decompress(const void * p_compressed_data, size_t compressed_len, const void * p_decompressed_data, size_t decompressed_len)
{
    uint8_t * p_out_buffer;
    size_t  out_buffer_len;
    size_t  out_length;


    out_buffer_len = decompressed_len + OUT_BUFFER_EXTRA_LEN;
    p_out_buffer = malloc(out_buffer_len);
    TEST_ASSERT_NOT_NULL(p_out_buffer);

    memset(p_out_buffer, 'A', out_buffer_len);

    out_length = lzs_decompress(p_out_buffer, out_buffer_len, p_compressed_data, compressed_len);

    TEST_ASSERT_EQUAL_size_t(decompressed_len, out_length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p_decompressed_data, p_out_buffer, out_length);

    free(p_out_buffer);
}

/**
 * \brief Test for lzs_decompress_incremental(), decompressing all in one go.
 */
static void test_lzs_decompress_incremental_all(const void * p_compressed_data, size_t compressed_len, const void * p_decompressed_data, size_t decompressed_len)
{
    uint8_t * p_out_buffer;
    size_t  out_buffer_len;
    LzsDecompressParameters_t   decompress_params;
    size_t  out_length;
    size_t  total_out_length = 0u;


    out_buffer_len = decompressed_len + OUT_BUFFER_EXTRA_LEN;
    p_out_buffer = malloc(out_buffer_len);
    TEST_ASSERT_NOT_NULL(p_out_buffer);

    memset(p_out_buffer, 'A', out_buffer_len);

    lzs_decompress_init(&decompress_params);

    // Decompress all in one go.
    // Actually, it will still stop at each end-marker.
    decompress_params.inPtr = p_compressed_data;
    decompress_params.inLength = compressed_len;
    decompress_params.outPtr = p_out_buffer;
    // out buffer length is '-1' to allow for string zero termination
    decompress_params.outLength = out_buffer_len - 1;
    while (1)
    {
        if (
                (decompress_params.inLength == 0) &&
                ((decompress_params.status & LZS_D_STATUS_INPUT_STARVED) != 0)
           )
        {
            break;
        }
        out_length = lzs_decompress_incremental(&decompress_params);
        total_out_length += out_length;
        // printf("lzs_decompress_incremental() exit with status %02X\n", decompress_params.status);
    }
    TEST_ASSERT_EQUAL_size_t(decompressed_len, total_out_length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p_decompressed_data, p_out_buffer, total_out_length);

    free(p_out_buffer);
}


/**
 * \brief Test for lzs_decompress_incremental(), calling multiple times, bounded by the input buffer size.
 */
static void test_lzs_decompress_incremental_input_bounded(const void * p_compressed_data, size_t compressed_len, const void * p_decompressed_data, size_t decompressed_len)
{
    uint8_t * p_out_buffer;
    size_t  out_buffer_len;
    LzsDecompressParameters_t   decompress_params;
    size_t  out_length;
    size_t  total_out_length = 0u;


    out_buffer_len = decompressed_len + OUT_BUFFER_EXTRA_LEN;
    p_out_buffer = malloc(out_buffer_len);
    TEST_ASSERT_NOT_NULL(p_out_buffer);

    memset(p_out_buffer, 'A', out_buffer_len);

    lzs_decompress_init(&decompress_params);

    // Decompress bounded by input buffer size
    decompress_params.inPtr = p_compressed_data;
    decompress_params.inLength = compressed_len;
    decompress_params.outPtr = p_out_buffer;
    // out buffer length is '-1' to allow for string zero termination
    decompress_params.outLength = out_buffer_len - 1;
    while (1)
    {
        decompress_params.inLength = (const uint8_t *)p_compressed_data + compressed_len - (const uint8_t *)decompress_params.inPtr;
        if (decompress_params.inLength > IN_BUFFER_BOUNDED_LEN)
            decompress_params.inLength = IN_BUFFER_BOUNDED_LEN;
        if (
                (decompress_params.inLength == 0) &&
                ((decompress_params.status & LZS_D_STATUS_INPUT_STARVED) != 0)
           )
        {
            break;
        }

        out_length = lzs_decompress_incremental(&decompress_params);
        TEST_ASSERT_LESS_OR_EQUAL_size_t(out_buffer_len, out_length);
        total_out_length += out_length;

#if 0
        // Add string zero termination
        *decompress_params.outPtr = 0;
        printf("    %s\n", decompress_params.outPtr - out_length);
        if ((decompress_params.status & ~(LZS_D_STATUS_INPUT_STARVED | LZS_D_STATUS_INPUT_FINISHED)) != 0)
        {
            printf("input bounded lzs_decompress_incremental() exit with status %02X\n", decompress_params.status);
        }
#endif
    }
    TEST_ASSERT_EQUAL_size_t(decompressed_len, total_out_length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p_decompressed_data, p_out_buffer, total_out_length);

    free(p_out_buffer);
}

/**
 * \brief Test for lzs_decompress_incremental(), calling multiple times, bounded by the output buffer size.
 */
static void test_lzs_decompress_incremental_output_bounded(const void * p_compressed_data, size_t compressed_len, const void * p_decompressed_data, size_t decompressed_len)
{
    uint8_t *   p_out_buffer;
    size_t      out_buffer_len;
    LzsDecompressParameters_t   decompress_params;
    size_t      out_length;
    size_t      total_out_length = 0u;


    out_buffer_len = decompressed_len + OUT_BUFFER_EXTRA_LEN;
    p_out_buffer = malloc(out_buffer_len);
    TEST_ASSERT_NOT_NULL(p_out_buffer);

    memset(p_out_buffer, 'A', out_buffer_len);

    lzs_decompress_init(&decompress_params);

    // Decompress bounded by output buffer size
    decompress_params.inPtr = p_compressed_data;
    decompress_params.inLength = compressed_len;
    decompress_params.outPtr = p_out_buffer;
    // out buffer length is '-1' to allow for string zero termination
    decompress_params.outLength = out_buffer_len - 1;
    while (1)
    {
        decompress_params.outLength = p_out_buffer + out_buffer_len - decompress_params.outPtr;
        if (decompress_params.outLength > OUT_BUFFER_BOUNDED_LEN)
            decompress_params.outLength = OUT_BUFFER_BOUNDED_LEN;
        if (
                (decompress_params.inLength == 0) &&
                ((decompress_params.status & LZS_D_STATUS_INPUT_STARVED) != 0)
           )
        {
            break;
        }

        out_length = lzs_decompress_incremental(&decompress_params);
        TEST_ASSERT_LESS_OR_EQUAL_size_t(OUT_BUFFER_BOUNDED_LEN, out_length);
        total_out_length += out_length;

#if 0
        // Add string zero termination
        *decompress_params.outPtr = 0;
        printf("    %s\n", decompress_params.outPtr - out_length);
        if ((decompress_params.status & ~LZS_D_STATUS_NO_OUTPUT_BUFFER_SPACE) != 0)
        {
            printf("output bounded lzs_decompress_incremental() exit with status %02X\n", decompress_params.status);
        }
#endif
    }
    TEST_ASSERT_EQUAL_size_t(decompressed_len, total_out_length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p_decompressed_data, p_out_buffer, total_out_length);

    free(p_out_buffer);
}

/**
 * \brief Test decompression functions, using data set of compressed_data_1[].
 */
void test_compressed_data_1(void)
{
    const uint8_t * p_compressed_data = "";
    const uint8_t * p_decompressed_data = "";
    size_t compressed_len = 0;
    size_t decompressed_len = 0;

    p_compressed_data = compressed_data_1;
    compressed_len = sizeof(compressed_data_1);
    p_decompressed_data = decompressed_data_1;
    decompressed_len = sizeof(decompressed_data_1) - 1u;

    test_lzs_decompress(p_compressed_data, compressed_len, p_decompressed_data, decompressed_len);
    test_lzs_decompress_incremental_all(p_compressed_data, compressed_len, p_decompressed_data, decompressed_len);
    test_lzs_decompress_incremental_input_bounded(p_compressed_data, compressed_len, p_decompressed_data, decompressed_len);
    test_lzs_decompress_incremental_output_bounded(p_compressed_data, compressed_len, p_decompressed_data, decompressed_len);
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_compressed_data_1);

    return UNITY_END();
}

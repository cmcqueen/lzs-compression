/*****************************************************************************
 *
 * \file
 *
 * \brief Unit Tests for Embedded Compression and Decompression
 *
 ****************************************************************************/


/*****************************************************************************
 * Includes
 ****************************************************************************/

#include "compression.h"

#include <stdio.h>
#include <string.h>         /* For memset() */


/*****************************************************************************
 * Tables
 ****************************************************************************/

static const uint8_t compressed_data[] =
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


/*****************************************************************************
 * Functions
 ****************************************************************************/

void test_decompress_1(void)
{
    uint8_t out_buffer[1000];
    size_t  out_length;


    memset(out_buffer, 'A', sizeof(out_buffer));

    // out buffer length is '-1' to allow for string zero termination
    out_length = decompress(out_buffer, sizeof(out_buffer) - 1, compressed_data, sizeof(compressed_data));

    // Add string zero termination
    out_buffer[out_length] = 0;
    printf("Decompressed data:\n%s\n", out_buffer);
}

void test_decompress_incremental_all(void)
{
    uint8_t out_buffer[1000];
    uint8_t history_buffer[MAX_HISTORY_SIZE];
    DecompressParameters_t  decompress_params;
    size_t  out_length;


    memset(out_buffer, 'A', sizeof(out_buffer));

    // Initialise
    decompress_params.historyPtr = history_buffer;
    decompress_params.historyBufferSize = sizeof(history_buffer);
    decompress_init(&decompress_params);

    // Decompress all in one go.
    // Actually, it will still stop at each end-marker.
    decompress_params.inPtr = compressed_data;
    decompress_params.inLength = sizeof(compressed_data);
    decompress_params.outPtr = out_buffer;
    // out buffer length is '-1' to allow for string zero termination
    decompress_params.outLength = sizeof(out_buffer) - 1;
    while (1)
    {
        if (
                (decompress_params.inLength == 0) &&
                ((decompress_params.status & D_STATUS_INPUT_STARVED) != 0)
           )
        {
            break;
        }
        out_length = decompress_incremental(&decompress_params);
        printf("Exit with status %02X\n", decompress_params.status);
    }
    // Add string zero termination
    *decompress_params.outPtr = 0;
    printf("Decompressed data:\n%s\n", out_buffer);
}


void test_decompress_incremental_input_bounded(void)
{
    uint8_t out_buffer[1000];
    uint8_t history_buffer[MAX_HISTORY_SIZE];
    DecompressParameters_t  decompress_params;
    size_t  out_length;


    memset(out_buffer, 'A', sizeof(out_buffer));

    // Initialise
    decompress_params.historyPtr = history_buffer;
    decompress_params.historyBufferSize = sizeof(history_buffer);
    decompress_init(&decompress_params);

    // Decompress bounded by input buffer size
    decompress_params.inPtr = compressed_data;
    decompress_params.outPtr = out_buffer;
    // out buffer length is '-1' to allow for string zero termination
    decompress_params.outLength = sizeof(out_buffer) - 1;
    while (1)
    {
        decompress_params.inLength = compressed_data + sizeof(compressed_data) - decompress_params.inPtr;
        if (decompress_params.inLength > 10u)
            decompress_params.inLength = 10u;
        if (
                (decompress_params.inLength == 0) &&
                ((decompress_params.status & D_STATUS_INPUT_STARVED) != 0)
           )
        {
            break;
        }

        out_length = decompress_incremental(&decompress_params);
        // Add string zero termination
        *decompress_params.outPtr = 0;
        printf("    %s\n", decompress_params.outPtr - out_length);
        if ((decompress_params.status & ~(D_STATUS_INPUT_STARVED | D_STATUS_INPUT_FINISHED)) != 0)
        {
            printf("Exit with status %02X\n", decompress_params.status);
        }
    }
    // Add string zero termination
    *decompress_params.outPtr = 0;
    printf("Decompressed data:\n%s\n", out_buffer);
}

void test_decompress_incremental_output_bounded(void)
{
    uint8_t out_buffer[1000];
    uint8_t history_buffer[MAX_HISTORY_SIZE];
    DecompressParameters_t  decompress_params;
    size_t  out_length;


    memset(out_buffer, 'A', sizeof(out_buffer));

    // Initialise
    decompress_params.historyPtr = history_buffer;
    decompress_params.historyBufferSize = sizeof(history_buffer);
    decompress_init(&decompress_params);

    // Decompress bounded by output buffer size
    decompress_params.inPtr = compressed_data;
    decompress_params.inLength = sizeof(compressed_data);
    decompress_params.outPtr = out_buffer;
    // out buffer length is '-1' to allow for string zero termination
    decompress_params.outLength = sizeof(out_buffer) - 1;
    while (1)
    {
        decompress_params.outLength = out_buffer + sizeof(out_buffer) - decompress_params.outPtr;
        if (decompress_params.outLength > 10u)
            decompress_params.outLength = 10u;
        if (
                (decompress_params.inLength == 0) &&
                ((decompress_params.status & D_STATUS_INPUT_STARVED) != 0)
           )
        {
            break;
        }

        out_length = decompress_incremental(&decompress_params);
        // Add string zero termination
        *decompress_params.outPtr = 0;
        printf("    %s\n", decompress_params.outPtr - out_length);
        if ((decompress_params.status & ~D_STATUS_NO_OUTPUT_BUFFER_SPACE) != 0)
        {
            printf("status %02X\n", decompress_params.status);
        }
    }
    // Add string zero termination
    *decompress_params.outPtr = 0;
    printf("Decompressed data:\n%s\n", out_buffer);
}

int main(int argc, char **argv)
{
#if 0
    test_decompress_1();
#elif 0
    test_decompress_incremental_all();
#elif 0
    test_decompress_incremental_input_bounded();
#elif 1
    test_decompress_incremental_output_bounded();
#endif

    return 0;
}


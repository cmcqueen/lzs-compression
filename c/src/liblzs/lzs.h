/*****************************************************************************
 *
 * \file
 *
 * \brief LZS Compression and Decompression
 *
 * This implements LZS (Lempel-Ziv-Stac) compression and decompression, which
 * is an LZ77 derived algorithm with a 2kB sliding window and Huffman coding.
 *
 * See:
 *     * ANSI X3.241-1994
 *     * RFC 1967
 *     * RFC 1974
 *     * RFC 2395
 *     * RFC 3943
 *
 * This code is licensed according to the MIT license as follows:
 * ----------------------------------------------------------------------------
 * Copyright (c) 2017 Craig McQueen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------------------------------------
 ****************************************************************************/

#ifndef __LZS_H
#define __LZS_H

/*****************************************************************************
 * Includes
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>


/*****************************************************************************
 * API Defines
 ****************************************************************************/

// LZS_MAX_HISTORY_SIZE is derived from LONG_OFFSET_BITS.
#define LZS_MAX_HISTORY_SIZE        ((1u << 11u) - 1u)

// Size to use for history buffer for incremental compression.
// In future, it may be different from LZS_MAX_HISTORY_SIZE, because we may
// also store look-ahead data in the history buffer.
#define LZS_COMPRESS_HISTORY_SIZE   LZS_MAX_HISTORY_SIZE

#define LZS_DECOMPRESS_HISTORY_SIZE LZS_MAX_HISTORY_SIZE

// Worst-case size of LZS compressed data, given input data of size X.
// Worst case is 9/8 times original size, plus a couple of bytes for end marker.
#define LZS_COMPRESSED_MAX(X)       ((X) + ((X) + 7u) / 8u + 3u)

// Worst-case size of LZS decompressed data, given compressed input data of
// size X. Worst case is 16 times original size.
#define LZS_DECOMPRESSED_MAX(X)     ((X) * 16u)


/*****************************************************************************
 * Implementation Defines
 ****************************************************************************/

#define LZS_SEARCH_BUF_LEN     15u


/*****************************************************************************
 * Typedefs
 ****************************************************************************/

typedef enum
{
    LZS_C_STATUS_NONE                   = 0x00,
    LZS_C_STATUS_INPUT_STARVED          = 0x01,
    LZS_C_STATUS_INPUT_FINISHED         = 0x02,
    LZS_C_STATUS_END_MARKER             = 0x04,
    LZS_C_STATUS_NO_OUTPUT_BUFFER_SPACE = 0x08,
    LZS_C_STATUS_ERROR                  = 0x10
} LzsCompressStatus_t;

typedef struct
{
    /*
     * These parameters should be set (as needed) each time prior to calling decompress_incremental().
     * Then, they are updated appropriately by decompress_incremental(), according to
     * what happens during the decompression process.
     */
    const uint8_t     * inPtr;              // On entry, points to input data. On exit, points to first unprocessed input data
    uint8_t           * outPtr;             // On entry, point to output data buffer. On exit, points to one past the last output data byte
    size_t              inLength;           // On entry, set this to the length of the input data. On exit, it is the length of unprocessed data
    size_t              outLength;          // On entry, set this to the space in the output buffer. On exit, decremented by the number of output bytes generated

    uint_fast8_t        status;

    /*
     * These are private members, and should not be changed.
     */
    uint8_t             historyBuffer[LZS_COMPRESS_HISTORY_SIZE];
    uint8_t             inSearchBuffer[LZS_SEARCH_BUF_LEN];
    uint8_t             inSearchBufferLen;
    uint32_t            bitFieldQueue;      // Code assumes bits will disappear past MS-bit 31 when shifted left
    uint_fast8_t        bitFieldQueueLen;   // Number of bits in the queue
    uint_fast16_t       historyReadIdx;
    uint_fast16_t       historyLatestIdx;
    uint_fast16_t       historyLen;
    uint_fast16_t       offset;
    uint_fast8_t        state;              // LzsCompressState_t
} LzsCompressParameters_t;


typedef enum
{
    LZS_D_STATUS_NONE                   = 0x00,
    LZS_D_STATUS_INPUT_STARVED          = 0x01,
    LZS_D_STATUS_INPUT_FINISHED         = 0x02,
    LZS_D_STATUS_END_MARKER             = 0x04,
    LZS_D_STATUS_NO_OUTPUT_BUFFER_SPACE = 0x08,
    LZS_D_STATUS_ERROR                  = 0x10
} LzsDecompressStatus_t;

typedef struct
{
    /*
     * These parameters should be set (as needed) each time prior to calling decompress_incremental().
     * Then, they are updated appropriately by decompress_incremental(), according to
     * what happens during the decompression process.
     */
    const uint8_t     * inPtr;              // On entry, points to input data. On exit, points to first unprocessed input data
    uint8_t           * outPtr;             // On entry, point to output data buffer. On exit, points to one past the last output data byte
    size_t              inLength;           // On entry, set this to the length of the input data. On exit, it is the length of unprocessed data
    size_t              outLength;          // On entry, set this to the space in the output buffer. On exit, decremented by the number of output bytes generated

    uint_fast8_t        status;

    /*
     * These are private members, and should not be changed.
     */
    uint8_t             historyBuffer[LZS_DECOMPRESS_HISTORY_SIZE];
    uint32_t            bitFieldQueue;      // Code assumes bits will disappear past MS-bit 31 when shifted left
    uint_fast8_t        bitFieldQueueLen;   // Number of bits in the queue
    uint_fast16_t       historyReadIdx;
    uint_fast16_t       historyLatestIdx;
    uint_fast16_t       offset;
    uint_fast8_t        length;
    uint_fast8_t        state;              // LzsDecompressState_t
} LzsDecompressParameters_t;


/*****************************************************************************
 * Function prototypes
 ****************************************************************************/

size_t lzs_compress(uint8_t * a_pOutData, size_t a_outBufferSize, const uint8_t * a_pInData, size_t a_inLen);

void lzs_compress_init(LzsCompressParameters_t * pParams);
size_t lzs_compress_incremental(LzsCompressParameters_t * pParams, bool add_end_marker);

size_t lzs_decompress(uint8_t * a_pOutData, size_t a_outBufferSize, const uint8_t * a_pInData, size_t a_inLen);

void lzs_decompress_init(LzsDecompressParameters_t * pParams);
size_t lzs_decompress_incremental(LzsDecompressParameters_t * pParams);


#endif // !defined(__LZS_H)

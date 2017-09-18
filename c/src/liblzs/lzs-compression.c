/*****************************************************************************
 *
 * \file
 *
 * \brief LZS Compression
 *
 * This implements LZS (Lempel-Ziv-Stac) compression, which is an LZ77
 * derived algorithm with a 2kB sliding window and Huffman coding.
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


/*****************************************************************************
 * Includes
 ****************************************************************************/

#include "lzs.h"

#include <stdint.h>

//#include <inttypes.h>
//#include <ctype.h>
//#include <stdio.h>

#include <string.h>


/*****************************************************************************
 * Defines
 ****************************************************************************/

#define SHORT_OFFSET_BITS           7u
#define LONG_OFFSET_BITS            11u
#define EXTENDED_LENGTH_BITS        4u
#define BIT_QUEUE_BITS              32u

#define SHORT_OFFSET_MAX            ((1u << SHORT_OFFSET_BITS) - 1u)
#define LONG_OFFSET_MAX             ((1u << LONG_OFFSET_BITS) - 1u)

#if (LZS_MAX_HISTORY_SIZE < ((1u << LONG_OFFSET_BITS) - 1u))
#error LZS_MAX_HISTORY_SIZE is too small
#endif

#define LENGTH_MAX_BIT_WIDTH        4u
#define MIN_LENGTH                  2u
#define MAX_SHORT_LENGTH            8u
#define MAX_EXTENDED_LENGTH         ((1u << EXTENDED_LENGTH_BITS) - 1u)

#define LZS_SEARCH_MATCH_MAX        12u

//#define LZS_DEBUG(X)                printf X
#define LZS_DEBUG(X)

#define LZS_ASSERT(X)

#if LZS_MAX_LOOK_AHEAD_LEN < MAX_SHORT_LENGTH || LZS_MAX_LOOK_AHEAD_LEN < MAX_EXTENDED_LENGTH || LZS_MAX_LOOK_AHEAD_LEN < LZS_SEARCH_MATCH_MAX
#error LZS_MAX_LOOK_AHEAD_LEN is too small
#endif

#define INPUT_HASH_SIZE             (1u << 12u)

#define ARRAY_ENTRIES(a)            (sizeof(a)/sizeof((a)[0]))


/*****************************************************************************
 * Typedefs
 ****************************************************************************/

typedef enum
{
    COMPRESS_NORMAL,
    COMPRESS_EXTENDED
} SimpleCompressState_t;

typedef uint16_t    input_hash_t;


/*****************************************************************************
 * Tables
 ****************************************************************************/

/* Length is encoded as:
 *  0b00 --> 2
 *  0b01 --> 3
 *  0b10 --> 4
 *  0b1100 --> 5
 *  0b1101 --> 6
 *  0b1110 --> 7
 *  0b1111 xxxx --> 8 (extended)
 */
static const uint8_t length_value[MAX_SHORT_LENGTH + 1u] =
{
    0,
    0,
    0x0,
    0x1,
    0x2,
    0xC,
    0xD,
    0xE,
    0xF
};

static const uint8_t length_width[MAX_SHORT_LENGTH + 1u] =
{
    0,
    0,
    2,
    2,
    2,
    4,
    4,
    4,
    4,
};


/*****************************************************************************
 * Inline Functions
 ****************************************************************************/

// Return hash of two input bytes, modulo INPUT_HASH_SIZE.
static inline input_hash_t inputs_hash(uint8_t a, uint8_t b)
{
    return (((input_hash_t)a << 4u) ^ (input_hash_t)b) % INPUT_HASH_SIZE;
}

static inline uint_fast8_t lzs_match_len(const uint8_t * aPtr, const uint8_t * bPtr, uint_fast8_t matchMax)
{
    uint_fast8_t    len;


    for (len = 0; len < matchMax; len++)
    {
        if (*aPtr++ != *bPtr++)
        {
            return len;
        }
    }
    return len;
}

static inline uint_fast8_t lzs_inc_match_len(LzsCompressParameters_t * pParams, uint_fast16_t offset, uint_fast8_t matchMax)
{
    uint_fast16_t   historyReadIdx;
    uint_fast16_t   historyLookAheadIdx;
    uint_fast8_t    len;


    // This relies on calculation overflows wrapping as expected --
    // true as long as ints are unsigned.
    if (pParams->historyLatestIdx < offset)
    {
        // This relies on two overflows of uint (during the two subtractions) cancelling out to a sensible value
        offset -= sizeof(pParams->historyBuffer);
    }
    historyReadIdx = pParams->historyLatestIdx - offset;

    historyLookAheadIdx = pParams->historyLatestIdx;

    for (len = 0; len < matchMax; )
    {
        if (pParams->historyBuffer[historyLookAheadIdx] != pParams->historyBuffer[historyReadIdx])
        {
            return len;
        }
        ++len;

        ++historyLookAheadIdx;
        if (historyLookAheadIdx >= sizeof(pParams->historyBuffer))
        {
            historyLookAheadIdx = 0;
        }

        ++historyReadIdx;
        if (historyReadIdx >= sizeof(pParams->historyBuffer))
        {
            historyReadIdx = 0;
        }
    }
    return len;
}


/*****************************************************************************
 * Functions
 ****************************************************************************/

/*
 * Single-call compression
 *
 * No state is kept between calls. Compression is expected to complete in a single call.
 * It will stop if/when it reaches the end of either the input or the output buffer.
 */
size_t lzs_compress(uint8_t * a_pOutData, size_t a_outBufferSize, const uint8_t * a_pInData, size_t a_inLen)
{
    const uint8_t     * inPtr;
    uint8_t           * outPtr;
    uint16_t            hashTable[INPUT_HASH_SIZE];
    uint16_t            historyHash[LZS_MAX_HISTORY_SIZE];
    size_t              historyLen;
    size_t              inRemaining;        // Count of remaining bytes of input
    size_t              outCount;           // Count of output bytes that have been generated
    uint32_t            bitFieldQueue;      // Code assumes bits will disappear past MS-bit 31 when shifted left.
    input_hash_t        inputHash;
    uint_fast8_t        bitFieldQueueLen;
    uint_fast16_t       historyReadIdx;
    uint_fast16_t       historyLatestIdx;
    uint_fast16_t       offset;
    uint_fast8_t        matchMax;
    uint_fast8_t        length;
    uint_fast16_t       best_offset;
    uint_fast8_t        best_length;
    uint16_t            temp16;
    uint8_t             temp8;
    SimpleCompressState_t state;


    historyLen = 0;
    bitFieldQueue = 0;
    bitFieldQueueLen = 0;
    historyLatestIdx = 0;
    inPtr = a_pInData;
    outPtr = a_pOutData;
    inRemaining = a_inLen;
    outCount = 0;
    state = COMPRESS_NORMAL;

#if 0
    for (temp16 = 0; temp16 < ARRAY_ENTRIES(hashTable); temp16++)
    {
        hashTable[temp16] = (uint16_t)-1;
    }
    for (temp16 = 0; temp16 < ARRAY_ENTRIES(historyHash); temp16++)
    {
        historyHash[temp16] = (uint16_t)-1;
    }
#endif

    for (;;)
    {
        /* Copy output bits to output buffer */
        while (bitFieldQueueLen >= 8u)
        {
            if (outCount >= a_outBufferSize)
            {
                return outCount;
            }
            *outPtr++ = (bitFieldQueue >> (bitFieldQueueLen - 8u));
            bitFieldQueueLen -= 8u;
            outCount++;
        }
        if (inRemaining == 0 && state == COMPRESS_NORMAL)
        {
            /* Exit for loop when all input data is processed. */
            break;
        }

        switch (state)
        {
            case COMPRESS_NORMAL:
                /* Look for a match in history */
                best_length = 0;
                matchMax = (inRemaining < LZS_SEARCH_MATCH_MAX) ? inRemaining : LZS_SEARCH_MATCH_MAX;
                if (matchMax >= 2u)
                {
                    inputHash = inputs_hash(*inPtr, *(inPtr + 1));
                    historyReadIdx = hashTable[inputHash];
                    if (historyReadIdx <= ARRAY_ENTRIES(historyHash))
                    {
                        offset = historyLatestIdx;
                        if (offset <= historyReadIdx)
                        {
                            offset += ARRAY_ENTRIES(historyHash);
                        }
                        offset -= historyReadIdx;

                        for ( ; offset <= historyLen; )
                        {
                            length = lzs_match_len(inPtr, inPtr - offset, matchMax);
                            if (length > best_length && length >= MIN_LENGTH)
                            {
                                best_offset = offset;
                                best_length = length;
                                if (length >= LZS_SEARCH_MATCH_MAX)
                                {
                                    break;
                                }
                            }

                            historyReadIdx = historyLatestIdx;
                            if (historyReadIdx < offset)
                            {
                                historyReadIdx += ARRAY_ENTRIES(historyHash);
                            }
                            historyReadIdx -= offset;

                            temp16 = historyHash[historyReadIdx];
                            if (temp16 == 0 || temp16 > LZS_MAX_HISTORY_SIZE)
                            {
                                break;
                            }
                            offset += temp16;
                        }
                    }
                }
                /* Output */
                if (best_length == 0)
                {
                    /* Byte-literal */
                    /* Leading 0 bit indicates offset/length token.
                     * Following 8 bits are byte-literal. */
                    bitFieldQueue <<= 9u;
                    bitFieldQueue |= *inPtr;
                    bitFieldQueueLen += 9u;
                    length = 1u;
                    LZS_DEBUG(("Literal %c (%02X)\n", isprint(*inPtr) ? *inPtr : '?', *inPtr));
                }
                else
                {
                    LZS_DEBUG(("Best offset %"PRIuFAST16" length %"PRIuFAST8"\n", best_offset, best_length));
                    /* Offset/length token */
                    /* 1 bit indicates offset/length token */
                    bitFieldQueue <<= 1u;
                    bitFieldQueueLen++;
                    bitFieldQueue |= 1u;
                    /* Encode offset */
                    if (best_offset <= SHORT_OFFSET_MAX)
                    {
                        /* Short offset */
                        LZS_DEBUG(("Short offset %"PRIuFAST16"\n", best_offset));
                        bitFieldQueue <<= (1u + SHORT_OFFSET_BITS);
                        /* Initial 1 bit indicates short offset */
                        bitFieldQueue |= (1u << SHORT_OFFSET_BITS) | best_offset;
                        bitFieldQueueLen += (1u + SHORT_OFFSET_BITS);
                    }
                    else
                    {
                        /* Long offset */
                        LZS_DEBUG(("Long offset %"PRIuFAST16"\n", best_offset));
                        bitFieldQueue <<= (1u + LONG_OFFSET_BITS);
                        /* Initial 0 bit indicates long offset */
                        bitFieldQueue |= best_offset;
                        bitFieldQueueLen += (1u + LONG_OFFSET_BITS);
                    }
                    /* Encode length */
                    length = (best_length < MAX_SHORT_LENGTH) ? best_length : MAX_SHORT_LENGTH;
                    LZS_DEBUG(("Length %"PRIuFAST8"\n", length));
                    temp8 = length_width[length];
                    bitFieldQueue <<= temp8;
                    bitFieldQueue |= length_value[length];
                    bitFieldQueueLen += temp8;

                    if (length == MAX_SHORT_LENGTH)
                    {
                        state = COMPRESS_EXTENDED;
                    }
                }
                break;
            case COMPRESS_EXTENDED:
                matchMax = (inRemaining < MAX_EXTENDED_LENGTH) ? inRemaining : MAX_EXTENDED_LENGTH;
                length = lzs_match_len(inPtr, inPtr - best_offset, matchMax);
                LZS_DEBUG(("Extended length %"PRIuFAST8"\n", length));

                /* Encode length */
                bitFieldQueue <<= EXTENDED_LENGTH_BITS;
                bitFieldQueue |= length;
                bitFieldQueueLen += EXTENDED_LENGTH_BITS;

                if (length != MAX_EXTENDED_LENGTH)
                {
                    state = COMPRESS_NORMAL;
                }
                break;
        }
        // 'length' contains number of input bytes encoded.
        // Update inPtr and inRemaining accordingly.
        for (temp8 = 0; temp8 < length; temp8++)
        {
            inputHash = inputs_hash(*inPtr, *(inPtr + 1));
            inPtr++;

            temp16 = historyLatestIdx;
            if (temp16 <= hashTable[inputHash])
            {
                temp16 += ARRAY_ENTRIES(historyHash);
            }
            temp16 -= hashTable[inputHash];

            hashTable[inputHash] = historyLatestIdx;
            historyHash[historyLatestIdx] = temp16;
            historyLatestIdx++;
            if (historyLatestIdx >= ARRAY_ENTRIES(historyHash))
            {
                historyLatestIdx = 0;
            }
        }

        inRemaining -= length;

        historyLen += length;
        if (historyLen > LZS_MAX_HISTORY_SIZE)
        {
            historyLen = LZS_MAX_HISTORY_SIZE;
        }
    }
    /* Make end marker, which is like a short offset with value 0, padded out
     * with 0 to 7 extra zeros to reach a byte boundary. That is,
     * 0b110000000 */
    bitFieldQueue <<= (2u + SHORT_OFFSET_BITS + 7u);
    bitFieldQueueLen += (2u + SHORT_OFFSET_BITS + 7u);
    bitFieldQueue |= (3u << (SHORT_OFFSET_BITS + 7u));
    /* Copy output bits to output buffer */
    while (bitFieldQueueLen >= 8u)
    {
        if (outCount >= a_outBufferSize)
        {
            return outCount;
        }
        *outPtr++ = (bitFieldQueue >> (bitFieldQueueLen - 8u));
        bitFieldQueueLen -= 8u;
        outCount++;
    }
    return outCount;
}

/*
 * \brief Initialise incremental compression
 */
void lzs_compress_init(LzsCompressParameters_t * pParams)
{
    pParams->status = LZS_C_STATUS_NONE;

    pParams->lookAheadLen = 0;
    pParams->bitFieldQueue = 0;
    pParams->bitFieldQueueLen = 0;
    pParams->state = COMPRESS_NORMAL;
    pParams->historyLatestIdx = 0;
    pParams->historyLookAheadIdx = 0;
    pParams->historyLen = 0;
    pParams->offset = 0;
}

size_t lzs_compress_incremental(LzsCompressParameters_t * pParams, bool add_end_marker)
{
    size_t              outCount;           // Count of output bytes that have been generated
    uint_fast16_t       offset;
    uint_fast8_t        matchMax;
    uint_fast8_t        length;
    uint_fast16_t       best_offset;
    uint_fast8_t        best_length;
    uint_fast8_t        temp8;


    pParams->status = LZS_C_STATUS_NONE;
    outCount = 0;

    for (;;)
    {
        length = 0;
        // Write data from the bit field queue to output
        while (pParams->bitFieldQueueLen >= 8u)
        {
            // Check if we have space in the output buffer
            if (pParams->outLength == 0)
            {
                // We're out of space in the output buffer.
                // Set status, exit this inner copying loop, but maintain the current state.
                pParams->status |= LZS_C_STATUS_NO_OUTPUT_BUFFER_SPACE;
                break;
            }
            *pParams->outPtr++ = (pParams->bitFieldQueue >> (pParams->bitFieldQueueLen - 8u));
            pParams->outLength--;
            pParams->bitFieldQueueLen -= 8u;
            ++outCount;
        }
        if (pParams->bitFieldQueueLen > BIT_QUEUE_BITS)
        {
            // It is an error if we ever get here.
            LZS_ASSERT(0);
            pParams->status |= LZS_C_STATUS_ERROR | LZS_C_STATUS_NO_OUTPUT_BUFFER_SPACE;
        }

        // Check if we need to finish for whatever reason
        if (pParams->status != LZS_C_STATUS_NONE)
        {
            // Break out of the top-level loop
            break;
        }
        // Check if we've reached the end of our input data
        if (pParams->inLength == 0)
        {
            pParams->status |= LZS_C_STATUS_INPUT_FINISHED | LZS_C_STATUS_INPUT_STARVED;
            if (add_end_marker == false)
            {
                break;
            }
        }

        // Try to fill look-ahead buffer in history buffer
        temp8 = LZS_MAX_LOOK_AHEAD_LEN - pParams->lookAheadLen;
        if (temp8 > pParams->inLength)
        {
            temp8 = pParams->inLength;
        }
        // temp8 holds number of bytes that can be copied from input to look-ahead area of historyBuffer[].
        // Copy that number of bytes from input into look-ahead area of historyBuffer[].
        pParams->lookAheadLen += temp8;
        pParams->inLength -= temp8;
        while (temp8--)
        {
            pParams->historyBuffer[pParams->historyLookAheadIdx++] = *pParams->inPtr++;
            if (pParams->historyLookAheadIdx >= sizeof(pParams->historyBuffer))
            {
                pParams->historyLookAheadIdx = 0;
            }
        }

        // Process input data in a state machine
        switch (pParams->state)
        {
            case COMPRESS_NORMAL:
                if (add_end_marker)
                {
                    matchMax = 1;
                }
                else
                {
                    matchMax = LZS_SEARCH_MATCH_MAX;
                }
                if (pParams->lookAheadLen < matchMax)
                {
                    // We don't have enough input data, so we're done for now.
                    pParams->status |= LZS_C_STATUS_INPUT_STARVED;
                    break;
                }

                // Look for a match in history.
                best_length = 0;
                matchMax = (pParams->lookAheadLen < LZS_SEARCH_MATCH_MAX) ? pParams->lookAheadLen : LZS_SEARCH_MATCH_MAX;
                for (offset = 1; offset <= pParams->historyLen; offset++)
                {
                    length = lzs_inc_match_len(pParams, offset, matchMax);
                    if (length > best_length && length >= MIN_LENGTH)
                    {
                        best_offset = offset;
                        best_length = length;
                        if (length >= LZS_SEARCH_MATCH_MAX)
                        {
                            break;
                        }
                    }
                }
                /* Output */
                if (best_length == 0)
                {
                    /* Byte-literal */
                    /* Leading 0 bit indicates offset/length token.
                     * Following 8 bits are byte-literal. */
                    pParams->bitFieldQueue <<= 9u;
                    temp8 = pParams->historyBuffer[pParams->historyLatestIdx];
                    pParams->bitFieldQueue |= temp8;
                    pParams->bitFieldQueueLen += 9u;
                    length = 1u;
                    LZS_DEBUG(("Literal %c (%02X)\n", isprint(temp8) ? temp8 : '?', temp8));
                }
                else
                {
                    LZS_DEBUG(("Best offset %"PRIuFAST16" length %"PRIuFAST8"\n", best_offset, best_length));
                    /* Offset/length token */
                    /* 1 bit indicates offset/length token */
                    pParams->bitFieldQueue <<= 1u;
                    pParams->bitFieldQueueLen++;
                    pParams->bitFieldQueue |= 1u;
                    /* Encode offset */
                    if (best_offset <= SHORT_OFFSET_MAX)
                    {
                        /* Short offset */
                        LZS_DEBUG(("Short offset %"PRIuFAST16"\n", best_offset));
                        pParams->bitFieldQueue <<= (1u + SHORT_OFFSET_BITS);
                        /* Initial 1 bit indicates short offset */
                        pParams->bitFieldQueue |= (1u << SHORT_OFFSET_BITS) | best_offset;
                        pParams->bitFieldQueueLen += (1u + SHORT_OFFSET_BITS);
                    }
                    else
                    {
                        /* Long offset */
                        LZS_DEBUG(("Long offset %"PRIuFAST16"\n", best_offset));
                        pParams->bitFieldQueue <<= (1u + LONG_OFFSET_BITS);
                        /* Initial 0 bit indicates long offset */
                        pParams->bitFieldQueue |= best_offset;
                        pParams->bitFieldQueueLen += (1u + LONG_OFFSET_BITS);
                    }
                    /* Encode length */
                    length = (best_length < MAX_SHORT_LENGTH) ? best_length : MAX_SHORT_LENGTH;
                    LZS_DEBUG(("Length %"PRIuFAST8"\n", length));
                    temp8 = length_width[length];
                    pParams->bitFieldQueue <<= temp8;
                    pParams->bitFieldQueue |= length_value[length];
                    pParams->bitFieldQueueLen += temp8;

                    if (length == MAX_SHORT_LENGTH)
                    {
                        pParams->offset = best_offset;
                        pParams->state = COMPRESS_EXTENDED;
                    }
                }
                break;
            case COMPRESS_EXTENDED:
                if (add_end_marker == false)
                {
                    if (pParams->lookAheadLen < MAX_EXTENDED_LENGTH)
                    {
                        // We don't have enough input data, so we're done for now.
                        pParams->status |= LZS_C_STATUS_INPUT_STARVED;
                        break;
                    }
                }

                // Get next length of extended match.
                matchMax = (pParams->lookAheadLen < MAX_EXTENDED_LENGTH) ? pParams->lookAheadLen : MAX_EXTENDED_LENGTH;
                length = lzs_inc_match_len(pParams, pParams->offset, matchMax);
                LZS_DEBUG(("Extended length %"PRIuFAST8"\n", length));

                /* Encode length */
                pParams->bitFieldQueue <<= EXTENDED_LENGTH_BITS;
                pParams->bitFieldQueue |= length;
                pParams->bitFieldQueueLen += EXTENDED_LENGTH_BITS;

                if (length != MAX_EXTENDED_LENGTH)
                {
                    pParams->state = COMPRESS_NORMAL;
                }
                break;
        }
        // 'length' contains number of input bytes encoded.
        pParams->historyLatestIdx += length;
        if (pParams->historyLatestIdx >= sizeof(pParams->historyBuffer))
        {
            pParams->historyLatestIdx -= sizeof(pParams->historyBuffer);
        }
        pParams->historyLen += length;
        if (pParams->historyLen > LZS_MAX_HISTORY_SIZE)
        {
            pParams->historyLen = LZS_MAX_HISTORY_SIZE;
        }
        pParams->lookAheadLen -= length;
    }

    if (add_end_marker &&
        pParams->inLength == 0 &&
        pParams->state == COMPRESS_NORMAL &&
        pParams->lookAheadLen == 0 &&
        pParams->bitFieldQueueLen < 8u &&
        pParams->outLength >= (pParams->bitFieldQueueLen + 2u + SHORT_OFFSET_BITS + 7u) / 8u)
    {
        /* Make end marker, which is like a short offset with value 0, padded out
         * with 0 to 7 extra zeros to reach a byte boundary. That is,
         * 0b110000000 */
        pParams->bitFieldQueue <<= (2u + SHORT_OFFSET_BITS + 7u);
        pParams->bitFieldQueueLen += (2u + SHORT_OFFSET_BITS + 7u);
        pParams->bitFieldQueue |= (3u << (SHORT_OFFSET_BITS + 7u));
        /* Copy output bits to output buffer */
        while (pParams->bitFieldQueueLen >= 8u)
        {
            *pParams->outPtr++ = (pParams->bitFieldQueue >> (pParams->bitFieldQueueLen - 8u));
            pParams->outLength--;
            pParams->bitFieldQueueLen -= 8u;
            ++outCount;
        }
        pParams->bitFieldQueueLen = 0;
        pParams->status |= LZS_C_STATUS_END_MARKER;
    }

    return outCount;
}

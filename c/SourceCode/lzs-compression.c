/*****************************************************************************
 *
 * \file
 *
 * \brief Embedded Compression
 *
 ****************************************************************************/


/*****************************************************************************
 * Includes
 ****************************************************************************/

#include "lzs.h"

#include <stdint.h>


/*****************************************************************************
 * Defines
 ****************************************************************************/

#define SHORT_OFFSET_BITS           7u
#define LONG_OFFSET_BITS            11u
#define EXTENDED_LENGTH_BITS        4u
#define BIT_QUEUE_BITS              32u

#define SHORT_OFFSET_MAX            ((1u << SHORT_OFFSET_BITS) - 1u)
#define LONG_OFFSET_MAX             ((1u << LONG_OFFSET_BITS) - 1u)

#if (MAX_HISTORY_SIZE < (1u << LONG_OFFSET_BITS))
#error MAX_HISTORY_SIZE is too small
#endif

#define LENGTH_MAX_BIT_WIDTH        4u
#define MIN_LENGTH                  2u
#define MAX_SHORT_LENGTH            8u
#define MAX_EXTENDED_LENGTH         ((1u << EXTENDED_LENGTH_BITS) - 1u)

#define LZS_SEARCH_MATCH_MAX        12u

//#define LZS_DEBUG(X)    printf X
#define LZS_DEBUG(X)

#define LZS_ASSERT(X)


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
static const uint8_t length_value[] =
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

static const uint8_t length_width[] =
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
 * Typedefs
 ****************************************************************************/

typedef enum
{
    COMPRESS_NORMAL,
    COMPRESS_EXTENDED
} SimpleCompressState_t;


/*****************************************************************************
 * Inline Functions
 ****************************************************************************/

static inline uint_fast8_t lzs_match_len(const uint8_t * aPtr, const uint8_t * bPtr, int_fast8_t matchMax)
{
    uint_fast8_t    len;
    
    for (len = 0; len < matchMax; len++)
    {
        if (*aPtr != *bPtr)
        {
            return len;
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
    size_t              historyLen;
    size_t              inRemaining;        // Count of remaining bytes of input
    size_t              outCount;           // Count of output bytes that have been generated
    uint32_t            bitFieldQueue;      // Code assumes bits will disappear past MS-bit 31 when shifted left.
    int_fast8_t         bitFieldQueueLen;
    uint_fast16_t       offset;
    uint_fast8_t        matchMax;
    uint_fast8_t        length;
    uint_fast8_t        best_length;
    uint8_t             temp8;
    SimpleCompressState_t state;


    historyLen = 0;
    bitFieldQueue = 0;
    bitFieldQueueLen = 0;
    inPtr = a_pInData;
    outPtr = a_pOutData;
    inRemaining = a_inLen;
    outCount = 0;
    state = COMPRESS_NORMAL;

    for (;;)
    {
        /* Copy output bits to output buffer */
        while (bitFieldQueueLen >= 8u)
        {
            if (outCount >= a_outBufferSize)
            {
                return outCount;
            }
            *outPtr++ = (bitFieldQueue >> (bitFieldQueueLen - 8));
            bitFieldQueueLen -= 8u;
        }
        if (inRemaining == 0)
        {
            /* Exit for loop when all input data is processed. */
            break;
        }

        switch (state)
        {
            case COMPRESS_NORMAL:
                /* Look for a match in history */
                best_length = 0;
                for (offset = 1; offset < historyLen; offset++)
                {
                    matchMax = (inRemaining < LZS_SEARCH_MATCH_MAX) ? inRemaining : LZS_SEARCH_MATCH_MAX;
                    length = lzs_match_len(inPtr, inPtr - offset);
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
                bitFieldQueue <<= 1u;
                bitFieldQueueLen++;
                if (best_length == 0)
                {
                    /* Literal */
                    bitFieldQueue <<= 8u;
                    bitFieldQueue |= *inPtr++;
                    bitFieldQueueLen += 8u;
                    inRemaining--;
                    length = 1;
                }
                else
                {
                    /* Offset/length token */
                    /* 1 bit indicates offset/length token */
                    bitFieldQueue |= 1u;    
                    /* Encode offset */
                    if (best_offset <= SHORT_OFFSET_MAX)
                    {
                        /* Short offset */
                        bitFieldQueue <<= (1u + SHORT_OFFSET_BITS);
                        /* Initial 1 bit indicates short offset */
                        bitFieldQueue |= (1u << SHORT_OFFSET_BITS) | best_offset;
                        bitFieldQueueLen += (1u + SHORT_OFFSET_BITS);
                    }
                    else
                    {
                        /* Long offset */
                        bitFieldQueue <<= (1u + LONG_OFFSET_BITS);
                        /* Initial 0 bit indicates long offset */
                        bitFieldQueue |= best_offset;
                        bitFieldQueueLen += (1u + LONG_OFFSET_BITS);
                    }
                    /* Encode length */
                    length = (best_length < MAX_SHORT_LENGTH) ? best_length : MAX_SHORT_LENGTH;
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
                length = lzs_match_len(inPtr, inPtr - offset);

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
        inPtr += length;
        inRemaining -= length;

        historyLen += length;
        if (historyLen > MAX_HISTORY_SIZE)
        {
            historyLen = MAX_HISTORY_SIZE;
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
        *outPtr++ = (bitFieldQueue >> (bitFieldQueueLen - 8));
        bitFieldQueueLen -= 8u;
    }
    return outCount;
}


/*
 * \brief Initialise incremental compression
 */
void lzs_compress_init(CompressParameters_t * pParams)
{
    pParams->status = D_STATUS_NONE;
    pParams->bitFieldQueue = 0;
    pParams->bitFieldQueueLen = 0;
    pParams->state = DECOMPRESS_GET_TOKEN_TYPE;
    pParams->historyLatestIdx = 0;
    pParams->historySize = 0;
}


/*
 * \brief Incremental compression
 *
 * State is kept between calls, so compression can be done gradually, and flexibly
 * depending on the application's needs for input/output buffer handling.
 *
 * It will stop if/when it reaches the end of either the input or the output buffer.
 */
size_t lzs_compress_incremental(CompressParameters_t * pParams, bool add_end_marker)
{

}

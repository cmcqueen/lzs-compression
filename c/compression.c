
#include <stdint.h>
#include <stdio.h>
#include <string.h>         /* For memset() */


#define SHORT_OFFSET_BITS           7u
#define LONG_OFFSET_BITS            11u
#define BIT_QUEUE_BITS              32u

#define MAX_HISTORY_SIZE            (1u << LONG_OFFSET_BITS)

#define LENGTH_MAX_BIT_WIDTH        4u
#define MAX_EXTENDED_LENGTH         15u

#define LENGTH_DECODE_METHOD_CODE   0
#define LENGTH_DECODE_METHOD_TABLE  1u
// Choose which method to use
#define LENGTH_DECODE_METHOD        LENGTH_DECODE_METHOD_TABLE

#if LENGTH_DECODE_METHOD == LENGTH_DECODE_METHOD_TABLE
#define MAX_INITIAL_LENGTH          8u      // keep in sync with lengthDecodeTable[]

static const uint8_t lengthDecodeTable[(1u << LENGTH_MAX_BIT_WIDTH)] =
{
    /* Length is encoded as:
     *  0b00 --> 2
     *  0b01 --> 3
     *  0b10 --> 4
     *  0b1100 --> 5
     *  0b1101 --> 6
     *  0b1110 --> 7
     *  0b1111 xxxx --> 8 (extended)
     */
    /* Look at 4 bits. Map 0bWXYZ to a length value, and a number of bits actually used for symbol.
     * High 4 bits are length value. Low 4 bits are the width of the bit field.
     */
    0x22, 0x22, 0x22, 0x22,     // 0b00 --> 2
    0x32, 0x32, 0x32, 0x32,     // 0b01 --> 3
    0x42, 0x42, 0x42, 0x42,     // 0b10 --> 4
    0x54, 0x64, 0x74, 0x84,     // 0b11xy --> 5, 6, 7, and also 8 (see MAX_INITIAL_LENGTH) which goes into extended lengths
};
#endif


//#define DEBUG(X)    printf X
#define DEBUG(X)

#define ASSERT(X)


typedef enum
{
    DECOMPRESS_NORMAL,
    DECOMPRESS_EXTENDED,
} SimpleDecompressState_t;


/*
 * Single-call decompression
 *
 * No state is kept between calls. Decompression is expected to complete in a single call.
 * It will stop if/when it reaches the end of either the input or the output buffer.
 */
size_t decompress(uint8_t * a_pOutData, size_t a_outBufferSize, const uint8_t * a_pInData, size_t a_inLen)
{
    const uint8_t     * inPtr;
    uint8_t           * outPtr;
    size_t              inRemaining;        // Count of remaining bytes of input
    size_t              outCount;           // Count of output bytes that have been generated
    uint32_t            bitFieldQueue;      /* Code assumes bits will disappear past MS-bit 31 when shifted left. */
    uint_fast8_t        bitFieldQueueLen;
    uint_fast16_t       offset;
    uint_fast8_t        length;
    uint8_t             temp8;
    SimpleDecompressState_t state;


    bitFieldQueue = 0;
    bitFieldQueueLen = 0;
    inPtr = a_pInData;
    outPtr = a_pOutData;
    inRemaining = a_inLen;
    outCount = 0;
    state = DECOMPRESS_NORMAL;

    while ((inRemaining > 0) || (bitFieldQueueLen != 0))
    {
        /* Load more into the bit field queue */
        while ((inRemaining > 0) && (bitFieldQueueLen <= BIT_QUEUE_BITS - 8u))
        {
            bitFieldQueue |= (*inPtr++ << (BIT_QUEUE_BITS - 8u - bitFieldQueueLen));
            bitFieldQueueLen += 8u;
            DEBUG(("Load queue: %04X\n", bitFieldQueue));
            inRemaining--;
        }
        switch (state)
        {
            case DECOMPRESS_NORMAL:
                /* Get token-type bit */
                temp8 = (bitFieldQueue & (1u << (BIT_QUEUE_BITS - 1u))) ? 1u : 0;
                bitFieldQueue <<= 1u;
                bitFieldQueueLen--;
                if (temp8 == 0)
                {
                    /* Literal */
                    temp8 = (uint8_t) (bitFieldQueue >> (BIT_QUEUE_BITS - 8u));
                    bitFieldQueue <<= 8u;
                    bitFieldQueueLen -= 8u;
                    DEBUG(("Literal %c\n", temp8));
                    if (outCount < a_outBufferSize)
                    {
                        *outPtr++ = temp8;
                        outCount++;
                    }
                }
                else
                {
                    /* Offset+length token */
                    /* Decode offset */
                    temp8 = (bitFieldQueue & (1u << (BIT_QUEUE_BITS - 1u))) ? 1u : 0;
                    bitFieldQueue <<= 1u;
                    bitFieldQueueLen--;
                    if (temp8)
                    {
                        /* Short offset */
                        offset = bitFieldQueue >> (BIT_QUEUE_BITS - SHORT_OFFSET_BITS);
                        bitFieldQueue <<= SHORT_OFFSET_BITS;
                        bitFieldQueueLen -= SHORT_OFFSET_BITS;
                        if (offset == 0)
                        {
                            DEBUG(("End marker\n"));
                            /* Discard any bits that are fractions of a byte, to align with a byte boundary */
                            temp8 = bitFieldQueueLen % 8u;
                            bitFieldQueue <<= temp8;
                            bitFieldQueueLen -= temp8;
                        }
                    }
                    else
                    {
                        /* Long offset */
                        offset = bitFieldQueue >> (BIT_QUEUE_BITS - LONG_OFFSET_BITS);
                        bitFieldQueue <<= LONG_OFFSET_BITS;
                        bitFieldQueueLen -= LONG_OFFSET_BITS;
                    }
                    if (offset != 0)
                    {
                        /* Decode length and copy characters */
#if LENGTH_DECODE_METHOD == LENGTH_DECODE_METHOD_CODE
                        /* Length is encoded as:
                         *  0b00 --> 2
                         *  0b01 --> 3
                         *  0b10 --> 4
                         *  0b1100 --> 5
                         *  0b1101 --> 6
                         *  0b1110 --> 7
                         *  0b1111 xxxx --> 8 (extended)
                         */
                        /* Get 4 bits */
                        temp8 = (uint8_t) (bitFieldQueue >> (BIT_QUEUE_BITS - 4u));
                        if (temp8 < 0xC)    /* 0xC is 0b1100 */
                        {
                            /* Length of 2, 3 or 4, encoded in 2 bits */
                            length = (temp8 >> 2u) + 2u;
                            bitFieldQueue <<= 2u;
                            bitFieldQueueLen -= 2u;
                        }
                        else
                        {
                            /* Length (encoded in 4 bits) of 5, 6, 7, or (8 + extended) */
                            length = (temp8 - 0xC + 5u);
                            bitFieldQueue <<= 4u;
                            bitFieldQueueLen -= 4u;
                            if (length == 8u)
                            {
                                /* We must go into extended length decode mode */
                                state = DECOMPRESS_EXTENDED;
                            }
                        }
#endif
#if LENGTH_DECODE_METHOD == LENGTH_DECODE_METHOD_TABLE
                        /* Get 4 bits, then look up decode data */
                        temp8 = lengthDecodeTable[
                                                  (uint8_t) (bitFieldQueue >> (BIT_QUEUE_BITS - LENGTH_MAX_BIT_WIDTH))
                                                 ];
                        // Length value is in upper nibble
                        length = temp8 >> 4u;
                        // Number of bits for this length token is in the lower nibble
                        temp8 &= 0xF;
                        bitFieldQueue <<= temp8;
                        bitFieldQueueLen -= temp8;
                        if (length == MAX_INITIAL_LENGTH)
                        {
                            /* We must go into extended length decode mode */
                            state = DECOMPRESS_EXTENDED;
                        }
#endif
                        DEBUG(("(%d, %d)\n", offset, length));
                        /* Now copy (offset, length) bytes */
                        for (temp8 = 0; temp8 < length; temp8++)
                        {
                            if (outCount < a_outBufferSize)
                            {
                                *outPtr = *(outPtr - offset);
                                ++outPtr;
                                ++outCount;
                            }
                        }
                    }
                }
                break;

            case DECOMPRESS_EXTENDED:
                /* Extended length token */
                /* Get 4 bits */
                length = (uint8_t) (bitFieldQueue >> (BIT_QUEUE_BITS - LENGTH_MAX_BIT_WIDTH));
                bitFieldQueue <<= LENGTH_MAX_BIT_WIDTH;
                bitFieldQueueLen -= LENGTH_MAX_BIT_WIDTH;
                /* Now copy (offset, length) bytes */
                for (temp8 = 0; temp8 < length; temp8++)
                {
                    if (outCount < a_outBufferSize)
                    {
                        *outPtr = *(outPtr - offset);
                        ++outPtr;
                        ++outCount;
                    }
                }
                if (length != MAX_EXTENDED_LENGTH)
                {
                    /* We're finished with extended length decode mode; go back to normal */
                    state = DECOMPRESS_NORMAL;
                }
                break;
        }
    }

    return outCount;
}


typedef enum
{
    DECOMPRESS_COPY_DATA,           // Must come before DECOMPRESS_GET_TOKEN_TYPE, so state transition can be done by increment
    DECOMPRESS_GET_TOKEN_TYPE,
    DECOMPRESS_GET_LITERAL,
    DECOMPRESS_GET_OFFSET_TYPE,
    DECOMPRESS_GET_OFFSET_SHORT,
    DECOMPRESS_GET_OFFSET_LONG,
    DECOMPRESS_GET_LENGTH,
    DECOMPRESS_COPY_EXTENDED_DATA,  // Must come before DECOMPRESS_GET_EXTENDED_LENGTH, so state transition can be done by increment
    DECOMPRESS_GET_EXTENDED_LENGTH,

    NUM_DECOMPRESS_STATES
} DecompressState_t;

const uint8_t StateBitMinimumWidth[NUM_DECOMPRESS_STATES] =
{
    0,                          // DECOMPRESS_COPY_DATA,
    1u,                         // DECOMPRESS_GET_TOKEN_TYPE,
    8u,                         // DECOMPRESS_GET_LITERAL,
    1u,                         // DECOMPRESS_GET_OFFSET_TYPE,
    SHORT_OFFSET_BITS,          // DECOMPRESS_GET_OFFSET_SHORT,
    LONG_OFFSET_BITS,           // DECOMPRESS_GET_OFFSET_LONG,
    0,                          // DECOMPRESS_GET_LENGTH,
    0,                          // DECOMPRESS_COPY_EXTENDED_DATA,
    LENGTH_MAX_BIT_WIDTH,       // DECOMPRESS_GET_EXTENDED_LENGTH,
};

typedef struct
{
    /*
     * These parameters should be set before calling the decompress_init(), and then not changed.
     */
    uint8_t           * historyPtr;         // Points to start of history buffer. Must be set at initialisation.
    uint_fast16_t       historyBufferSize;  // The size of the history buffer. Must be set at initialisation.

    /*
     * These parameters should be set (as needed) each time prior to calling decompress_incremental().
     * Then, they are updated appropriately by decompress_incremental(), according to
     * what happens during the decompression process.
     */
    const uint8_t     * inPtr;              // On entry, points to input data. On exit, points to first unprocessed input data
    uint8_t           * outPtr;             // On entry, point to output data buffer. On exit, points to one past the last output data byte
    size_t              inLength;           // On entry, set this to the length of the input data. On exit, it is the length of unprocessed data
    size_t              outLength;          // On entry, set this to the space in the output buffer. On exit, decremented by the number of output bytes generated

    // TODO: add a status flag

    /*
     * These are private members, and should not be changed.
     */
    uint32_t            bitFieldQueue;      // Code assumes bits will disappear past MS-bit 31 when shifted left
    uint_fast8_t        bitFieldQueueLen;   // Number of bits in the queue
    uint_fast16_t       historyReadIdx;
    uint_fast16_t       historyLatestIdx;
    uint_fast16_t       historySize;
    uint_fast16_t       offset;
    uint_fast8_t        length;
    DecompressState_t   state;
} DecompressParameters_t;


/*
 * Initialise incremental decompression
 */
void decompress_init(DecompressParameters_t * pParams)
{
    pParams->bitFieldQueue = 0;
    pParams->bitFieldQueueLen = 0;
    pParams->state = DECOMPRESS_GET_TOKEN_TYPE;
    pParams->historyLatestIdx = 0;
    pParams->historySize = 0;
}

/*
 * Incremental decompression
 *
 * State is kept between calls. Decompression is expected to complete in a single call.
 * It will stop if/when it reaches the end of either the input or the output buffer.
 */
size_t decompress_incremental(DecompressParameters_t * pParams)
{
    size_t              outCount;           // Count of output bytes that have been generated
    uint_fast16_t       offset;
    uint_fast8_t        temp8;


    outCount = 0;

    while (
            (pParams->outLength > 0) &&
            ((pParams->inLength > 0) || (pParams->bitFieldQueueLen != 0))
          )
    {
        /* Load more into the bit field queue */
        while ((pParams->inLength > 0) && (pParams->bitFieldQueueLen <= BIT_QUEUE_BITS - 8u))
        {
            pParams->bitFieldQueue |= (*pParams->inPtr++ << (BIT_QUEUE_BITS - 8u - pParams->bitFieldQueueLen));
            pParams->bitFieldQueueLen += 8u;
            DEBUG(("Load queue: %04X\n", pParams->bitFieldQueue));
            pParams->inLength--;
        }
        if (pParams->bitFieldQueueLen < StateBitMinimumWidth[pParams->state])
        {
            // We don't have enough input bits, so we're done for now.
            return outCount;
        }
        switch (pParams->state)
        {
            case DECOMPRESS_GET_TOKEN_TYPE:
                /* Get token-type bit */
                if (pParams->bitFieldQueue & (1u << (BIT_QUEUE_BITS - 1u)))
                {
                    pParams->state = DECOMPRESS_GET_OFFSET_TYPE;
                }
                else
                {
                    pParams->state = DECOMPRESS_GET_LITERAL;
                }
                pParams->bitFieldQueue <<= 1u;
                pParams->bitFieldQueueLen--;
                break;

            case DECOMPRESS_GET_LITERAL:
                /* Literal */
                temp8 = (uint8_t) (pParams->bitFieldQueue >> (BIT_QUEUE_BITS - 8u));
                pParams->bitFieldQueue <<= 8u;
                pParams->bitFieldQueueLen -= 8u;
                DEBUG(("Literal %c\n", temp8));

                *pParams->outPtr++ = temp8;
                pParams->outLength--;
                outCount++;

                // Write to history
                pParams->historyPtr[pParams->historyLatestIdx] = temp8;

                // Increment write index, and wrap if necessary
                pParams->historyLatestIdx++;
                if (pParams->historyLatestIdx >= pParams->historyBufferSize)
                {
                    pParams->historyLatestIdx = 0;
                }

                pParams->state = DECOMPRESS_GET_TOKEN_TYPE;
                break;

            case DECOMPRESS_GET_OFFSET_TYPE:
                /* Offset+length token */
                /* Decode offset */
                temp8 = (pParams->bitFieldQueue & (1u << (BIT_QUEUE_BITS - 1u))) ? 1u : 0;
                pParams->bitFieldQueue <<= 1u;
                pParams->bitFieldQueueLen--;
                if (temp8)
                {
                    pParams->state = DECOMPRESS_GET_OFFSET_SHORT;
                }
                else
                {
                    pParams->state = DECOMPRESS_GET_OFFSET_LONG;
                }
                break;

            case DECOMPRESS_GET_OFFSET_SHORT:
                /* Short offset */
                offset = pParams->bitFieldQueue >> (BIT_QUEUE_BITS - SHORT_OFFSET_BITS);
                pParams->bitFieldQueue <<= SHORT_OFFSET_BITS;
                pParams->bitFieldQueueLen -= SHORT_OFFSET_BITS;
                if (offset == 0)
                {
                    DEBUG(("End marker\n"));
                    /* Discard any bits that are fractions of a byte, to align with a byte boundary */
                    temp8 = pParams->bitFieldQueueLen % 8u;
                    pParams->bitFieldQueue <<= temp8;
                    pParams->bitFieldQueueLen -= temp8;
                    pParams->state = DECOMPRESS_GET_TOKEN_TYPE;
                }
                else
                {
                    pParams->offset = offset;
                    pParams->state = DECOMPRESS_GET_LENGTH;
                }
                break;

        case DECOMPRESS_GET_OFFSET_LONG:
                /* Long offset */
                pParams->offset = pParams->bitFieldQueue >> (BIT_QUEUE_BITS - LONG_OFFSET_BITS);
                pParams->bitFieldQueue <<= LONG_OFFSET_BITS;
                pParams->bitFieldQueueLen -= LONG_OFFSET_BITS;

                pParams->state = DECOMPRESS_GET_LENGTH;
                break;

            case DECOMPRESS_GET_LENGTH:
                /* Decode length and copy characters */
#if LENGTH_DECODE_METHOD == LENGTH_DECODE_METHOD_CODE
                // TODO: fill this in
#endif
#if LENGTH_DECODE_METHOD == LENGTH_DECODE_METHOD_TABLE
                /* Get 4 bits, then look up decode data */
                temp8 = lengthDecodeTable[
                                          pParams->bitFieldQueue >> (BIT_QUEUE_BITS - LENGTH_MAX_BIT_WIDTH)
                                         ];
                // Length value is in upper nibble
                pParams->length = temp8 >> 4u;
                // Number of bits for this length token is in the lower nibble
                temp8 &= 0xF;
#endif
                if (pParams->bitFieldQueueLen < temp8)
                {
                    // We don't have enough input bits, so we're done for now.
                    return outCount;
                }
                else
                {
                    pParams->bitFieldQueue <<= temp8;
                    pParams->bitFieldQueueLen -= temp8;
                    if (pParams->length == MAX_INITIAL_LENGTH)
                    {
                        /* We must go into extended length decode mode */
                        pParams->state = DECOMPRESS_COPY_EXTENDED_DATA;
                    }
                    else
                    {
                        pParams->state = DECOMPRESS_COPY_DATA;
                    }

                    // Do some offset calculations before beginning to copy
                    offset = pParams->offset;
                    ASSERT(offset < pParams->historyBufferSize);
    #if 0
                    // This code is a "safe" version (no overflows as long as pParams->historyBufferSize < MAX_UINT16/2)
                    if (offset > pParams->historyLatestIdx)
                    {
                        pParams->historyReadIdx = pParams->historyLatestIdx + pParams->historyBufferSize - offset;
                    }
                    else
                    {
                        pParams->historyReadIdx = pParams->historyLatestIdx - offset;
                    }
    #else
                    // This code is simpler, but relies on calculation overflows wrapping as expected.
                    if (offset > pParams->historyLatestIdx)
                    {
                        /* This relies on two overflows of uint (during the two subtractions) cancelling out to a sensible value */
                        offset -= pParams->historyBufferSize;
                    }
                    pParams->historyReadIdx = pParams->historyLatestIdx - offset;
    #endif
                    DEBUG(("(%d, %d)\n", offset, pParams->length));
                }
                break;

            case DECOMPRESS_COPY_DATA:
            case DECOMPRESS_COPY_EXTENDED_DATA:
                /* Now copy (offset, length) bytes */
                if (pParams->length == 0)
                {
                    pParams->state++;   // Goes to either DECOMPRESS_GET_TOKEN_TYPE or DECOMPRESS_GET_EXTENDED_LENGTH
                }
                else
                {
                    // Get byte from history
                    temp8 = (pParams->historyPtr)[pParams->historyReadIdx];

                    // Increment read index, and wrap if necessary
                    pParams->historyReadIdx++;
                    if (pParams->historyReadIdx >= pParams->historyBufferSize)
                    {
                        pParams->historyReadIdx = 0;
                    }

                    // Write to output
                    *pParams->outPtr++ = temp8;
                    pParams->outLength--;
                    pParams->length--;
                    ++outCount;

                    // Write to history
                    pParams->historyPtr[pParams->historyLatestIdx] = temp8;

                    // Increment write index, and wrap if necessary
                    pParams->historyLatestIdx++;
                    if (pParams->historyLatestIdx >= pParams->historyBufferSize)
                    {
                        pParams->historyLatestIdx = 0;
                    }
                }
                break;

            case DECOMPRESS_GET_EXTENDED_LENGTH:
                /* Extended length token */
                /* Get 4 bits */
                pParams->length = (uint8_t) (pParams->bitFieldQueue >> (BIT_QUEUE_BITS - LENGTH_MAX_BIT_WIDTH));
                pParams->bitFieldQueue <<= LENGTH_MAX_BIT_WIDTH;
                pParams->bitFieldQueueLen -= LENGTH_MAX_BIT_WIDTH;
                if (pParams->length == MAX_EXTENDED_LENGTH)
                {
                    /* We stay in extended length decode mode */
                    pParams->state = DECOMPRESS_COPY_EXTENDED_DATA;
                }
                else
                {
                    /* We're finished with extended length decode mode; go back to normal */
                    pParams->state = DECOMPRESS_COPY_DATA;
                }
                break;

            default:
                // TODO: It is an error if we ever get here. Need to do some handling.
                ASSERT(0);
                break;
        }
    }

    return outCount;
}


int main(int argc, char **argv)
{
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
            0x0C, 0xEC, 0x77, 0xF8, 0x87, 0x63, 0xB4, 0x21,
            0x52, 0xFB, 0x4E, 0xC7, 0xAC, 0x55, 0x82, 0x0C,
            0x21, 0xC3, 0x64, 0x38, 0xDC, 0x31, 0x10, 0xC6,
            0x12, 0x31, 0xC8, 0xDF, 0xD4, 0x21, 0xF2, 0x42,
            0x79, 0x8A, 0xFF, 0xB4, 0x52, 0xF7, 0x22, 0xC6,
#if 1
            0x15, 0xFD, 0x98, 0x82, 0x16, 0xEC, 0x2F, 0x0C,
            0xE1, 0x17, 0x34, 0xC5, 0x19, 0x10, 0xC8, 0x65,
            0x33, 0x47, 0x33, 0xC0, 0xBE, 0x5F, 0x88, 0x8D,
            0x86, 0x22, 0x52, 0x46, 0x48, 0x85, 0x8A, 0x6F,
            0x32, 0x0B, 0xB0, 0x00,
#endif
        };
    uint8_t out_buffer[1000];
    uint8_t history_buffer[MAX_HISTORY_SIZE];
    size_t  out_length;
    DecompressParameters_t  decompress_params;


    memset(out_buffer, 'A', sizeof(out_buffer));
#if 0
    // out buffer length is '-1' to allow for string zero termination
    out_length = decompress(out_buffer, sizeof(out_buffer) - 1, compressed_data, sizeof(compressed_data));

    // Add string zero termination
    out_buffer[out_length] = 0;
    printf("%s", out_buffer);
#else

    // Initialise
    decompress_params.historyPtr = history_buffer;
    decompress_params.historyBufferSize = sizeof(history_buffer);
    decompress_init(&decompress_params);

#if 0
    // Decompress all in one go
    decompress_params.inPtr = compressed_data;
    decompress_params.inLength = sizeof(compressed_data);
    decompress_params.outPtr = out_buffer;
    // out buffer length is '-1' to allow for string zero termination
    decompress_params.outLength = sizeof(out_buffer) - 1;
    out_length = decompress_incremental(&decompress_params);
    // Add string zero termination
    out_buffer[out_length] = 0;
    printf("%s", out_buffer);
#elif 1
    // Decompress bounded by input buffer size
    decompress_params.inPtr = compressed_data;
    while (1)
    {
        decompress_params.inLength = compressed_data + sizeof(compressed_data) - decompress_params.inPtr;
        if (decompress_params.inLength > 10u)
            decompress_params.inLength = 10u;
        if (decompress_params.inLength == 0)
            break;

        decompress_params.outPtr = out_buffer;
        // out buffer length is '-1' to allow for string zero termination
        decompress_params.outLength = sizeof(out_buffer) - 1;
        out_length = decompress_incremental(&decompress_params);
        // Add string zero termination
        out_buffer[out_length] = 0;
        printf("%s\n", out_buffer);
    }
#endif
#endif

    return 0;
}



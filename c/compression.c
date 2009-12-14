
#include <stdint.h>
#include <stdio.h>


#define SHORT_OFFSET_BITS           7u
#define LONG_OFFSET_BITS            11u
#define BIT_QUEUE_BITS              32u

typedef enum
{
    DECOMPRESS_NORMAL,
    DECOMPRESS_GET_EXTENDED_LENGTH,
} DecompressState_t;


#if 0
static const uint8_t lengthDecodeTable[] =
{
    /* Look at 4 bits. Map 0bWXYZ to a length value, and a number of bits actually used for symbol.
     * High 4 bits are length value. Low 4 bits are the width of the bit field.
     */
    0x22, 0x22, 0x22, 0x22,     // 0b00 --> 2
    0x32, 0x32, 0x32, 0x32,     // 0b01 --> 3
    0x42, 0x42, 0x42, 0x42,     // 0b10 --> 4
    0x54, 0x64, 0x74, 0x84,     // 0b11xy --> 5, 6, 7, and also 8 which goes into extended lengths
};
#endif

//#define DEBUG(X)    printf X
#define DEBUG(X)

void decompress(uint8_t * a_pOutData, size_t a_outBufferSize, const uint8_t * a_pInData, size_t a_inLen)
{
    const uint8_t     * inPtr;
    uint8_t           * outPtr;
    size_t              inCount;
    size_t              outCount;
    uint32_t            bitFieldQueue;      /* Code assumes bits will disappear past MS-bit 31 when shifted left. */
    uint_fast8_t        bitFieldQueueLen;
    uint_fast16_t       offset;
    uint_fast8_t        length;
    uint8_t             temp8;
    DecompressState_t   state;


    bitFieldQueue = 0;
    bitFieldQueueLen = 0;
    inPtr = a_pInData;
    outPtr = a_pOutData;
    inCount = 0;
    outCount = 0;
    state = DECOMPRESS_NORMAL;

    while ((inCount < a_inLen) || (bitFieldQueueLen != 0))
    {
        /* Load more into the bit field queue */
        while ((inCount < a_inLen) && (bitFieldQueueLen <= BIT_QUEUE_BITS - 8u))
        {
            DEBUG(("Loading queue\n"));
            bitFieldQueue |= (*inPtr++ << (BIT_QUEUE_BITS - 8u - bitFieldQueueLen));
            bitFieldQueueLen += 8u;
            DEBUG(("%04X\n", bitFieldQueue));
            inCount++;
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
                    DEBUG(("Literal %c\n", temp8));
                    bitFieldQueue <<= 8u;
                    bitFieldQueueLen -= 8u;
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
                                state = DECOMPRESS_GET_EXTENDED_LENGTH;
                            }
                        }
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

            case DECOMPRESS_GET_EXTENDED_LENGTH:
                /* Extended length token */
                /* Get 4 bits */
                length = (uint8_t) (bitFieldQueue >> (BIT_QUEUE_BITS - 4u));
                bitFieldQueue <<= 4u;
                bitFieldQueueLen -= 4u;
                /* Now copy (offset, length) bytes */
                for (temp8 = 0; temp8 < length; temp8++)
                {
                    *outPtr = *(outPtr - offset);
                    ++outPtr;
                }
                if (length != 15u)
                {
                    /* We're finished with extended length decode mode; go back to normal */
                    state = DECOMPRESS_NORMAL;
                }
                break;
        }
    }
}


int main()
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
            0x15, 0xFD, 0x98, 0x82, 0x16, 0xEC, 0x2F, 0x0C,
            0xE1, 0x17, 0x34, 0xC5, 0x19, 0x10, 0xC8, 0x65,
            0x33, 0x47, 0x33, 0xC0, 0xBE, 0x5F, 0x88, 0x8D,
            0x86, 0x22, 0x52, 0x46, 0x48, 0x85, 0x8A, 0x6F,
            0x32, 0x0B, 0xB0, 0x00,
        };
    uint8_t out_buffer[1000];
    decompress(out_buffer, sizeof(out_buffer), compressed_data, sizeof(compressed_data));
    printf("%s", out_buffer);
    return 0;
}




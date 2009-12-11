
#include <stdint.h>


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


void decompress(uint8_t * a_pOutData, size_t a_outBufferSize, const uint8_t * a_pInData, size_t a_inLen)
{
    const uint8_t     * inPtr;
    const uint8_t     * outPtr;
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
    outCount = 0;
    state = DECOMPRESS_NORMAL;

    while ((inCount < a_inLen) || (bitFieldQueueLen != 0))
    {
        /* Load more into the bit field queue */
        while ((inCount < a_inLen) && (bitFieldQueueLen <= BIT_QUEUE_BITS - 8u))
        {
            bitFieldQueue |= (*inPtr++ << (BIT_QUEUE_BITS - 8u - bitFieldQueueLen));
            bitFieldQueueLen += 8u;
            inCount++;
        }
        switch (state)
        {
            case DECOMPRESS_NORMAL:
                /* Get token-type bit */
                temp8 = (bitFieldQueue & (1u << (BIT_QUEUE_BITS - 1u))) ? 1u : 0;
                bitFieldQueue <<= 1u;
                bitFieldQueueLen--;
                if (temp8)
                {
                    /* Literal */
                    temp8 = (uint8_t) (bitFieldQueue >> (BIT_QUEUE_BITS - 8u));
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
                bitFieldQueue <<= 1u;
                bitFieldQueueLen -= 1u;
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

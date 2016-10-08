/*****************************************************************************
 *
 * \file
 *
 * \brief Embedded Compression and Decompression
 *
 ****************************************************************************/

#ifndef __LZS_DECOMPRESSION_H
#define __LZS_DECOMPRESSION_H

/*****************************************************************************
 * Includes
 ****************************************************************************/

#include <stdint.h>
#include <stdlib.h>


/*****************************************************************************
 * Defines
 ****************************************************************************/

// MAX_HISTORY_SIZE is derived from LONG_OFFSET_BITS.
#define MAX_HISTORY_SIZE            (1u << 11u)


/*****************************************************************************
 * Typedefs
 ****************************************************************************/

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

typedef enum
{
    D_STATUS_NONE                       = 0x00,
    D_STATUS_INPUT_STARVED              = 0x01,
    D_STATUS_INPUT_FINISHED             = 0x02,
    D_STATUS_END_MARKER                 = 0x04,
    D_STATUS_NO_OUTPUT_BUFFER_SPACE     = 0x08
} DecompressStatus_t;


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

    uint_fast8_t        status;

    /*
     * These are private members, and should not be changed.
     */
    uint32_t            bitFieldQueue;      // Code assumes bits will disappear past MS-bit 31 when shifted left
    int_fast8_t         bitFieldQueueLen;   // Number of bits in the queue
    uint_fast16_t       historyReadIdx;
    uint_fast16_t       historyLatestIdx;
    uint_fast16_t       historySize;
    uint_fast16_t       offset;
    uint_fast8_t        length;
    DecompressState_t   state;
} DecompressParameters_t;


/*****************************************************************************
 * Function prototypes
 ****************************************************************************/

size_t decompress(uint8_t * a_pOutData, size_t a_outBufferSize, const uint8_t * a_pInData, size_t a_inLen);

void decompress_init(DecompressParameters_t * pParams);
size_t decompress_incremental(DecompressParameters_t * pParams);


#endif // !defined(__LZS_DECOMPRESSION_H)

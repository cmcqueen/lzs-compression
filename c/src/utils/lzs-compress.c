/*****************************************************************************
 *
 * \file
 *
 * \brief Compression of a file
 *
 ****************************************************************************/


/*****************************************************************************
 * Includes
 ****************************************************************************/

#include "lzs.h"

#include <stdio.h>
#include <string.h>         /* For memset() */

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


/*****************************************************************************
 * Defines
 ****************************************************************************/

#define LZS_USE_INCREMENTAL         1
#define LZS_USE_SIMPLE_ALGORITHM    0

#define INCREMENTAL_INPUT_SIZE      512
#define INCREMENTAL_OUTPUT_SIZE     512


/*****************************************************************************
 * Functions
 ****************************************************************************/

#if LZS_USE_INCREMENTAL

/*
 * Use incremental version of the compression algorithm.
 */
int main(int argc, char **argv)
{
    int in_fd;
    int out_fd;
    ssize_t read_len;
    ssize_t write_len;
    uint8_t in_buffer[INCREMENTAL_INPUT_SIZE];
    uint8_t out_buffer[INCREMENTAL_OUTPUT_SIZE];
#if LZS_USE_SIMPLE_ALGORITHM
    LzsSimpleCompressParameters_t   compress_params;
#else
    LzsCompressParameters_t         compress_params;
#endif
    size_t  out_length;
    bool    finish = false;

    if (argc < 3)
    {
        printf("Too few arguments\n");
        exit(1);
    }
    in_fd = open(argv[1], O_RDONLY);
    if (in_fd < 0)
    {
        perror("argv[1]");
        exit(2);
    }
    out_fd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (out_fd < 0)
    {
        perror("argv[2]");
        exit(3);
    }

    // Initialise
#if LZS_USE_SIMPLE_ALGORITHM
    lzs_simple_compress_init(&compress_params);
#else
    lzs_compress_init(&compress_params);
#endif

    // Compress bounded by input buffer size
    compress_params.inPtr = in_buffer;
    compress_params.inLength = 0;
    compress_params.outPtr = out_buffer;
    compress_params.outLength = sizeof(out_buffer);
    finish = false;
    while ((compress_params.status & LZS_C_STATUS_END_MARKER) == 0)
    {
        if (compress_params.inLength == 0 && finish == false)
        {
            read_len = read(in_fd, in_buffer, sizeof(in_buffer));
            if (read_len < 0)
            {
                perror("read");
                exit(4);
            }
            compress_params.inPtr = in_buffer;
            compress_params.inLength = read_len;
        }
        if (
                (compress_params.inLength == 0) &&
                ((compress_params.status & LZS_C_STATUS_INPUT_STARVED) != 0)
           )
        {
            finish = true;
        }

#if LZS_USE_SIMPLE_ALGORITHM
        out_length = lzs_simple_compress_incremental(&compress_params, finish);
#else
        out_length = lzs_compress_incremental(&compress_params, finish);
#endif
        if (out_length)
        {
            write_len = write(out_fd, compress_params.outPtr - out_length, out_length);
            if (write_len < 0)
            {
                perror("write");
                exit(5);
            }
            compress_params.outPtr = out_buffer;
            compress_params.outLength = sizeof(out_buffer);
        }
#if 0
        if ((compress_params.status & ~(LZS_C_STATUS_INPUT_STARVED | LZS_C_STATUS_INPUT_FINISHED)) != 0)
        {
            printf("Exit with status %02X\n", compress_params.status);
        }
#endif
    }

    return 0;
}

#else

/*
 * Use single-call version of the compression algorithm.
 *
 * The whole of the source data must be loaded into memory as a single buffer.
 * The output also goes into a single output buffer in memory.
 */
int main(int argc, char **argv)
{
    int in_fd;
    int out_fd;
    struct stat stbuf;
    ssize_t read_len;
    ssize_t write_len;
    uint8_t * inBufferPtr = NULL;
    uint8_t * outBufferPtr = NULL;
    ssize_t inBufferSize;
    ssize_t outBufferSize;
    size_t  out_length;

    if (argc < 3)
    {
        printf("Too few arguments\n");
        exit(1);
    }
    in_fd = open(argv[1], O_RDONLY);
    if (in_fd < 0)
    {
        perror("argv[1]");
        exit(2);
    }
    out_fd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (out_fd < 0)
    {
        perror("argv[2]");
        exit(3);
    }

    if ((fstat(in_fd, &stbuf) != 0) || (!S_ISREG(stbuf.st_mode)))
    {
        perror("fstat");
        exit(4);
    }

    inBufferSize = stbuf.st_size;
    inBufferPtr = (uint8_t *)malloc(inBufferSize);
    if (inBufferPtr == NULL)
    {
        perror("malloc for input data");
        exit(5);
    }

    outBufferSize = LZS_COMPRESSED_MAX(stbuf.st_size);
    outBufferPtr = (uint8_t *)malloc(outBufferSize);
    if (outBufferPtr == NULL)
    {
        perror("malloc for output data");
        exit(6);
    }

    read_len = read(in_fd, inBufferPtr, stbuf.st_size);
    if (read_len != stbuf.st_size)
    {
        perror("read");
        exit(7);
    }

    out_length = lzs_compress(outBufferPtr, outBufferSize, inBufferPtr, inBufferSize);

    write_len = write(out_fd, outBufferPtr, out_length);
    if (write_len < 0)
    {
        perror("write");
        exit(8);
    }

    return 0;
}

#endif

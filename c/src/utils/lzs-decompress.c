/*****************************************************************************
 *
 * \file
 *
 * \brief Decompression of a file
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

#define INCREMENTAL_INPUT_SIZE      16
#define INCREMENTAL_OUTPUT_SIZE     16


/*****************************************************************************
 * Functions
 ****************************************************************************/

#if LZS_USE_INCREMENTAL

/*
 * Use incremental version of the decompression algorithm.
 *
 * The data is input and output in chunks.
 */
int main(int argc, char **argv)
{
    int in_fd;
    int out_fd;
    ssize_t read_len;
    ssize_t write_len;
    uint8_t in_buffer[INCREMENTAL_INPUT_SIZE];
    uint8_t out_buffer[INCREMENTAL_OUTPUT_SIZE];
    LzsDecompressParameters_t   decompress_params;
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

    // Initialise
    lzs_decompress_init(&decompress_params);

    // Decompress bounded by input buffer size
    decompress_params.inPtr = in_buffer;
    decompress_params.inLength = 0;
    decompress_params.outPtr = out_buffer;
    decompress_params.outLength = sizeof(out_buffer);
    while (1)
    {
        if (decompress_params.inLength == 0)
        {
            read_len = read(in_fd, in_buffer, sizeof(in_buffer));
            if (read_len < 0)
            {
                perror("read");
                exit(4);
            }
            decompress_params.inPtr = in_buffer;
            decompress_params.inLength = read_len;
        }
        if (
                (decompress_params.inLength == 0) &&
                ((decompress_params.status & LZS_D_STATUS_INPUT_STARVED) != 0)
           )
        {
            break;
        }

        out_length = lzs_decompress_incremental(&decompress_params);
        if (out_length)
        {
            write_len = write(out_fd, decompress_params.outPtr - out_length, out_length);
            if (write_len < 0)
            {
                perror("write");
                exit(5);
            }
            decompress_params.outPtr = out_buffer;
            decompress_params.outLength = sizeof(out_buffer);
        }
#if 0
        if ((decompress_params.status & ~(LZS_D_STATUS_INPUT_STARVED | LZS_D_STATUS_INPUT_FINISHED)) != 0)
        {
            printf("Exit with status %02X\n", decompress_params.status);
        }
#endif
    }

    return 0;
}

#else

/*
 * Use single-call version of the decompression algorithm.
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

    outBufferSize = LZS_DECOMPRESSED_MAX(stbuf.st_size);
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

    out_length = lzs_decompress(outBufferPtr, outBufferSize, inBufferPtr, inBufferSize);

    write_len = write(out_fd, outBufferPtr, out_length);
    if (write_len < 0)
    {
        perror("write");
        exit(8);
    }

    return 0;
}

#endif

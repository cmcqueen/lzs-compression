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


/*****************************************************************************
 * Functions
 ****************************************************************************/

#if 1

// Use lzs_decompress_incremental()
int main(int argc, char **argv)
{
    int in_fd;
    int out_fd;
    int read_len;
    uint8_t in_buffer[16];
    uint8_t out_buffer[16];
    uint8_t history_buffer[LZS_MAX_HISTORY_SIZE];
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
    memset(history_buffer, 0, sizeof(history_buffer));
    decompress_params.historyPtr = history_buffer;
    decompress_params.historyBufferSize = sizeof(history_buffer);
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
            if (read_len > 0)
            {
                decompress_params.inPtr = in_buffer;
                decompress_params.inLength = read_len;
            }
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
            write(out_fd, decompress_params.outPtr - out_length, out_length);
            decompress_params.outPtr = out_buffer;
            decompress_params.outLength = sizeof(out_buffer);
        }
        if ((decompress_params.status & ~(LZS_D_STATUS_INPUT_STARVED | LZS_D_STATUS_INPUT_FINISHED)) != 0)
        {
            printf("Exit with status %02X\n", decompress_params.status);
        }
    }

    return 0;
}

#else

// Use lzs_decompress()
int main(int argc, char **argv)
{
    int in_fd;
    int out_fd;
    int read_len;
    int total_read_len;
    uint8_t in_buffer[16];
    uint8_t * in_buffer_ptr;
    uint8_t * out_buffer_ptr;
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

    printf("get input len\n");
    total_read_len = 0;
    while (1)
    {
        read_len = read(in_fd, in_buffer, sizeof(in_buffer));
        if (read_len <= 0)
        {
            break;
        }
        total_read_len += read_len;
    }
    close(in_fd);
    printf("input len %d\n", total_read_len);

    in_buffer_ptr = malloc(total_read_len);
    if (in_buffer_ptr == NULL)
    {
        perror("malloc for in data");
        exit(4);
    }

    out_buffer_ptr = malloc(LZS_DECOMPRESSED_MAX(total_read_len));
    if (out_buffer_ptr == NULL)
    {
        perror("malloc for out data");
        exit(5);
    }

    in_fd = open(argv[1], O_RDONLY);
    if (in_fd < 0)
    {
        perror("argv[1]");
        exit(6);
    }

    read_len = read(in_fd, in_buffer_ptr, total_read_len);
    if (read_len != total_read_len)
    {
        printf("mismatch in read len\n");
        exit(7);
    }

    printf("decompress\n");
    out_length = lzs_decompress(out_buffer_ptr, LZS_DECOMPRESSED_MAX(total_read_len), in_buffer_ptr, total_read_len);
    printf("decompress done\n");
    write(out_fd, out_buffer_ptr, out_length);

    return 0;
}

#endif

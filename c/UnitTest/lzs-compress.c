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

/*****************************************************************************
 * Functions
 ****************************************************************************/

int main(int argc, char **argv)
{
    int in_fd;
    int out_fd;
    int read_len;
    uint8_t inBufferPtr = NULL;
    uint8_t outBufferPtr = NULL;
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

    /* TODO: malloc for input and output buffers */
    /* TODO: read all input data */
    /* TODO: call lzs_compress() */
    /* TODO: write compressed data to output file */

    return 0;
}

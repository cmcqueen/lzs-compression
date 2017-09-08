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
 * Functions
 ****************************************************************************/

int main(int argc, char **argv)
{
    int in_fd;
    int out_fd;
    struct stat stbuf;
    ssize_t read_len;
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

    write(out_fd, outBufferPtr, out_length);

    return 0;
}

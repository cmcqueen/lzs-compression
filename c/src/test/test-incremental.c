
#include <string.h>
#include <stdio.h>

#include "lzs.h"

static void compress_decompress_incremental()
{
    const char* in_buffer = "Return a string containing a printable representation of an object. "
      "For many types, this function makes an attempt to return a string that would yield an "
      "object with the same value when passed to eval(), otherwise the representation is a "
      "string enclosed in angle brackets that contains the name of the type of the object "
      "together with additional information often including the name and address of the object.";

    uint8_t out_buffer[1024];
    LzsCompressParameters_t     compress_params;
    size_t out_size = 0;

    lzs_compress_init(&compress_params);

    compress_params.inPtr = (const uint8_t*)in_buffer;
    compress_params.inLength = strlen(in_buffer);
    compress_params.outPtr = out_buffer;
    compress_params.outLength = sizeof(out_buffer);

    for (;;)
    {
        out_size += lzs_compress_incremental(&compress_params, true);
        printf("Compress status %02X\n", compress_params.status);
        if (compress_params.status & LZS_C_STATUS_END_MARKER)
        {
            break;
        }
    }

    char dec_buffer[1024];

    LzsDecompressParameters_t   decompress_params;
    lzs_decompress_init(&decompress_params);

    decompress_params.inPtr = out_buffer;
    decompress_params.inLength = out_size;
    decompress_params.outPtr = (uint8_t*)dec_buffer;
    decompress_params.outLength = sizeof(dec_buffer);

    size_t dec_size = lzs_decompress_incremental(&decompress_params);
    printf("Decompress status %02X\n", decompress_params.status);
    //dec_buffer[dec_size] = 0;

    printf("Decompressed data \n%.*s\n", dec_size, dec_buffer);
}

int main()
{
    compress_decompress_incremental();
    return 0;
}


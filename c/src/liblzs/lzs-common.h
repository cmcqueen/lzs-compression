/*****************************************************************************
 *
 * \file
 *
 * \brief Common declarations for LZS Compression and Decompression
 *
 * This code is licensed according to the MIT license as follows:
 * ----------------------------------------------------------------------------
 * Copyright (c) 2017 Craig McQueen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------------------------------------
 ****************************************************************************/

#ifndef __LZS_COMMON_H
#define __LZS_COMMON_H

/*****************************************************************************
 * Implementation Defines
 ****************************************************************************/

#define LZSMIN(X,Y)                 (((X) < (Y)) ? (X) : (Y))


/*****************************************************************************
 * Inline Functions
 ****************************************************************************/

static inline uint_fast16_t lzs_idx_inc_wrap(uint_fast16_t idx, uint_fast16_t inc, uint_fast16_t array_size)
{
    uint_fast16_t new_idx;

    new_idx = idx + inc;
    if (new_idx >= array_size)
    {
        new_idx -= array_size;
    }
    return new_idx;
}

static inline uint_fast16_t lzs_idx_dec_wrap(uint_fast16_t idx, uint_fast16_t dec, uint_fast16_t array_size)
{
    uint_fast16_t new_idx;

    // This relies on calculation overflows wrapping as expected --
    // true as long as ints are unsigned.
    if (idx < dec)
    {
        // This relies on two overflows of uint (during the two subtractions) canceling out to a sensible value.
        dec -= array_size;
    }
    new_idx = idx - dec;
    return new_idx;
}


#endif // !defined(__LZS_COMMON_H)

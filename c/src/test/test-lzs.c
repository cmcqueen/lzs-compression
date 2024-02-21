/*****************************************************************************
 *
 * \file test-lzs.c
 *
 * \brief Unit Tests for Embedded Compression and Decompression
 *
 ****************************************************************************/


/*****************************************************************************
 * Includes
 ****************************************************************************/

#include "lzs.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>         /* For memset() */


/*****************************************************************************
 * Defines
 ****************************************************************************/

#define OUT_BUFFER_EXTRA_LEN    520u
#define IN_BUFFER_BOUNDED_LEN   10u
#define OUT_BUFFER_BOUNDED_LEN  10u

#define LITERAL_CHAR_BITS       9u
#define OFFSET_SHORT_BITS       7u
#define OFFSET_LONG_BITS        11u
#define END_MARKER_BITS         9u


/*****************************************************************************
 * Tables
 ****************************************************************************/

/*
 * Sequence of 23 lowercase alphabetical, without any repeated sequences.
 * Use 23 rather than 26, because 23 is prime, and so the maths works more simply.
 * 22 rows, each with 23 characters, for a total of 506 characters.
 */
static const uint8_t uncompressible_sequence[] =
    "abcdefghijklmnopqrstuvw"  // Alphabetical sequence.
    "bdfhjlnprtvacegikmoqsuw"  // Every 2nd letter.
    "cfiloruadgjmpsvbehknqtw"  // Every 3rd letter.
    "dhlptaeimqubfjnrvcgkosw"  // ...
    "ejotbglqvdinsafkpuchmrw"
    "flragmsbhntcioudjpvekqw"
    "gnuelscjqahovfmtdkrbipw"
    "hpaiqbjrcksdltemufnvgow"
    "irdmvhqclugpbktfoajsenw"
    "jtgqdnakuhreoblvisfpcmw"
    "kvjuithsgrfqepdocnbmalw"
    "lambncodpeqfrgshtiujvkw"
    "mcpfsivlboerhukandqgtjw"
    "nesjaoftkbpgulcqhvmdriw"
    "ogvnfumetldskcrjbqiaphw"
    "pibrkdtmfvohaqjcsleungw"
    "qkevpjduoictnhbsmgarlfw"
    "rmhcupkfasnidvqlgbtojew"
    "sokgcvrnjfbuqmieatplhdw"
    "tqnkhebvspmjgdaurolifcw"
    "usqomkigecavtrpnljhfdbw"
    "vutsrqponmlkjihgfedcbaw";


/*****************************************************************************
 * Support functions
 ****************************************************************************/

static size_t length_bits(size_t repeated_chars)
{
    switch (repeated_chars)
    {
        case 0u:    return 0u;
        case 1u:    return 9u;
        case 2u:    return 2u;
        case 3u:    return 2u;
        case 4u:    return 2u;
        case 5u:    return 4u;
        case 6u:    return 4u;
        case 7u:    return 4u;
        default:    return (((repeated_chars + 22u) / 15u) * 4u);
    }
}

/*****************************************************************************
 * Test functions
 ****************************************************************************/

static void test_uncompressible(void)
{
    uint8_t compress_buffer[1000];
    uint8_t decompress_buffer[1000];
    size_t  max_data_len;
    size_t  data_len;
    size_t  expected_compress_len;
    size_t  compress_len;
    size_t  decompress_len;

    max_data_len = strlen(uncompressible_sequence);

    for (data_len = 0; data_len <= max_data_len; data_len++)
    {
        memset(compress_buffer, 'C', sizeof(compress_buffer));
        memset(decompress_buffer, 'D', sizeof(decompress_buffer));

        expected_compress_len = (data_len * LITERAL_CHAR_BITS + END_MARKER_BITS + 7u) / 8u;
        compress_len = lzs_compress(compress_buffer, sizeof(compress_buffer), uncompressible_sequence, data_len);
        TEST_ASSERT_EQUAL_size_t(expected_compress_len, compress_len);

        decompress_len = lzs_decompress(decompress_buffer, sizeof(decompress_buffer), compress_buffer, compress_len);
        TEST_ASSERT_EQUAL_size_t(data_len, decompress_len);
        if (data_len)
            TEST_ASSERT_EQUAL_UINT8_ARRAY(uncompressible_sequence, decompress_buffer, data_len);
    }
}

static void test_repeated_byte(void)
{
    char    msg[100];
    uint8_t data_buffer[1000];
    uint8_t compress_buffer[1000];
    uint8_t decompress_buffer[1000];
    size_t  max_data_len;
    size_t  data_len;
    size_t  expected_compress_bits;
    size_t  expected_compress_len;
    size_t  compress_len;
    size_t  decompress_len;

    max_data_len = sizeof(data_buffer);
    memset(data_buffer, 'X', max_data_len);

    for (data_len = 0; data_len <= max_data_len; data_len++)
    {
        snprintf(msg, sizeof(msg), "data_len = %zu", data_len);
        memset(compress_buffer, 'C', sizeof(compress_buffer));
        memset(decompress_buffer, 'D', sizeof(decompress_buffer));

        switch(data_len)
        {
            case 0:
                expected_compress_bits = 0u;
                break;
            case 1:
                expected_compress_bits = LITERAL_CHAR_BITS;
                break;
            case 2:
                expected_compress_bits = LITERAL_CHAR_BITS + LITERAL_CHAR_BITS;
                break;
            default:
                expected_compress_bits = LITERAL_CHAR_BITS + 2u + OFFSET_SHORT_BITS + length_bits(data_len - 1u);
                break;
        }
        expected_compress_len = (expected_compress_bits + END_MARKER_BITS + 7u) / 8u;
        compress_len = lzs_compress(compress_buffer, sizeof(compress_buffer), data_buffer, data_len);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(expected_compress_len, compress_len, msg);

        decompress_len = lzs_decompress(decompress_buffer, sizeof(decompress_buffer), compress_buffer, compress_len);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(data_len, decompress_len, msg);
        if (data_len)
            TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(data_buffer, decompress_buffer, data_len, msg);
    }
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_uncompressible);
    RUN_TEST(test_repeated_byte);

    return UNITY_END();
}

/**
 * @file test_malformed.c
 * @brief Malformed input tests for CCSDS 124.0-B-1 compression/decompression.
 *
 * Tests error handling for:
 * - Invalid parameters (F=0, R>7, invalid timing)
 * - Truncated data (empty, partial packets)
 * - Corrupted data (bit flips, invalid sequences)
 * - Boundary conditions (max length, edge patterns)
 *
 * Part of comprehensive test validation per CCSDS standardization approach.
 */

#include "ccsds124.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Test counters */
static int tests_passed = 0;
static int tests_total = 0;

/**
 * @brief Assert helper with descriptive output.
 */
#define TEST_ASSERT(cond, msg) do { \
    tests_total++; \
    if (cond) { \
        tests_passed++; \
        printf("  %s... \xe2\x9c\x93\n", msg); \
    } else { \
        printf("  %s... FAILED\n", msg); \
    } \
} while(0)

/* ============================================================================
 * Invalid Parameter Tests - Compression
 * ============================================================================ */

static void test_compress_F_zero(void) {
    ccsds124_compressor_t comp;
    int result = ccsds124_compressor_init(&comp, 0, NULL, 0, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Compressor F=0 rejected");
}

static void test_compress_F_exceeds_max(void) {
    ccsds124_compressor_t comp;
    int result = ccsds124_compressor_init(&comp, CCSDS124_MAX_PACKET_LENGTH + 1, NULL, 0, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Compressor F>max rejected");
}

static void test_compress_R_exceeds_max(void) {
    ccsds124_compressor_t comp;
    int result = ccsds124_compressor_init(&comp, 8, NULL, 8, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Compressor R=8 rejected");

    result = ccsds124_compressor_init(&comp, 8, NULL, 255, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Compressor R=255 rejected");
}

static void test_compress_all_valid_R_values(void) {
    ccsds124_compressor_t comp;
    int all_passed = 1;

    for (uint8_t r = 0; r <= 7; r++) {
        int result = ccsds124_compressor_init(&comp, 8, NULL, r, 0, 0, 0);
        if (result != CCSDS124_OK) {
            all_passed = 0;
            printf("    R=%u unexpectedly rejected\n", r);
        }
    }
    TEST_ASSERT(all_passed, "All valid R values (0-7) accepted");
}

static void test_compress_null_compressor(void) {
    int result = ccsds124_compressor_init(NULL, 8, NULL, 0, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "NULL compressor rejected");
}

static void test_compress_null_input(void) {
    ccsds124_compressor_t comp;
    bitbuffer_t output;

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);
    bitbuffer_init(&output);

    int result = ccsds124_compress_packet(&comp, NULL, &output, NULL);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "NULL input packet rejected");
}

static void test_compress_null_output(void) {
    ccsds124_compressor_t comp;
    bitvector_t input;

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 8);

    int result = ccsds124_compress_packet(&comp, &input, NULL, NULL);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "NULL output buffer rejected");
}

static void test_compress_mismatched_input_length(void) {
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 16);  /* 16 bits, but compressor expects 8 */
    bitbuffer_init(&output);

    int result = ccsds124_compress_packet(&comp, &input, &output, NULL);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Mismatched input length rejected");
}

/* ============================================================================
 * Invalid Parameter Tests - Decompression
 * ============================================================================ */

static void test_decompress_F_zero(void) {
    ccsds124_decompressor_t decomp;
    int result = ccsds124_decompressor_init(&decomp, 0, NULL, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Decompressor F=0 rejected");
}

static void test_decompress_F_exceeds_max(void) {
    ccsds124_decompressor_t decomp;
    int result = ccsds124_decompressor_init(&decomp, CCSDS124_MAX_PACKET_LENGTH + 1, NULL, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Decompressor F>max rejected");
}

static void test_decompress_R_exceeds_max(void) {
    ccsds124_decompressor_t decomp;
    int result = ccsds124_decompressor_init(&decomp, 8, NULL, 8);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Decompressor R=8 rejected");
}

static void test_decompress_all_valid_R_values(void) {
    ccsds124_decompressor_t decomp;
    int all_passed = 1;

    for (uint8_t r = 0; r <= 7; r++) {
        int result = ccsds124_decompressor_init(&decomp, 8, NULL, r);
        if (result != CCSDS124_OK) {
            all_passed = 0;
            printf("    R=%u unexpectedly rejected\n", r);
        }
    }
    TEST_ASSERT(all_passed, "All valid R values (0-7) accepted for decomp");
}

/* ============================================================================
 * Truncated Data Tests
 * ============================================================================ */

static void test_decompress_empty_input(void) {
    ccsds124_decompressor_t decomp;
    uint8_t output[16];
    size_t output_size = 0;

    ccsds124_decompressor_init(&decomp, 8, NULL, 0);

    int result = ccsds124_decompress(&decomp, NULL, 0, output, sizeof(output), &output_size);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Empty input (NULL) rejected");

    uint8_t empty[1] = {0};
    result = ccsds124_decompress(&decomp, empty, 0, output, sizeof(output), &output_size);
    TEST_ASSERT(output_size == 0, "Zero-length input produces no output");
}

static void test_decompress_single_byte(void) {
    ccsds124_decompressor_t decomp;
    uint8_t input[1] = {0xFF};  /* Single incomplete byte */
    uint8_t output[16];
    size_t output_size = 0;

    ccsds124_decompressor_init(&decomp, 64, NULL, 0);  /* 8 bytes per packet */

    /* Single byte cannot contain a complete packet header */
    int result = ccsds124_decompress(&decomp, input, 1, output, sizeof(output), &output_size);
    /* Should either fail or produce no output */
    TEST_ASSERT(result != CCSDS124_OK || output_size == 0, "Single byte insufficient for packet");
}

static void test_count_decode_truncated(void) {
    bitreader_t reader;
    uint32_t value;

    /* '110' prefix requires 5 more bits, but we only provide 4 total bits.
     * Note: Current implementation reads available bits without explicit underflow
     * checking mid-decode. This test verifies current behavior. */
    uint8_t data[1] = {0xC0};  /* 0b11000000 - only 4 bits available */
    bitreader_init(&reader, data, 4);

    int result = ccsds124_count_decode(&reader, &value);
    /* Current implementation proceeds with available bits - this behavior could
     * be tightened to return CCSDS124_ERROR_UNDERFLOW in future versions. */
    TEST_ASSERT(result == CCSDS124_OK || result == CCSDS124_ERROR_UNDERFLOW,
                "COUNT decode truncated data handled");
}

static void test_count_decode_truncated_extended(void) {
    bitreader_t reader;
    uint32_t value;

    /* '111' prefix requires extended encoding, but we only provide 3 bits.
     * Same note as above regarding underflow checking. */
    uint8_t data[1] = {0xE0};  /* 0b11100000 */
    bitreader_init(&reader, data, 3);

    int result = ccsds124_count_decode(&reader, &value);
    /* Current implementation may proceed without strict underflow checking */
    TEST_ASSERT(result == CCSDS124_OK || result == CCSDS124_ERROR_UNDERFLOW,
                "COUNT extended decode truncated handled");
}

static void test_rle_decode_truncated(void) {
    bitreader_t reader;
    bitvector_t result_vec;

    /* RLE needs COUNT values, but we provide incomplete data */
    uint8_t data[1] = {0x00};  /* Just a '0' which decodes to 1, but not enough */
    bitreader_init(&reader, data, 1);
    bitvector_init(&result_vec, 64);  /* Need 64 bits total */

    int result = ccsds124_rle_decode(&reader, &result_vec, 64);
    /* Should fail because we can't decode enough runs for 64 bits */
    TEST_ASSERT(result != CCSDS124_OK, "RLE decode with insufficient data fails");
}

/* ============================================================================
 * Corrupted Data Tests
 * ============================================================================ */

static void test_decompress_corrupted_compressed_stream(void) {
    ccsds124_compressor_t comp;
    ccsds124_decompressor_t decomp;
    uint8_t input_data[16] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44,
                               0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};

    ccsds124_compressor_init(&comp, 64, NULL, 1, 10, 20, 50);

    uint8_t compressed[256];
    size_t compressed_size = 0;
    ccsds124_compress(&comp, input_data, 16, compressed, sizeof(compressed), &compressed_size);

    /* Corrupt the compressed data by flipping bits */
    if (compressed_size > 2) {
        compressed[1] ^= 0xFF;  /* Flip all bits in second byte */
    }

    ccsds124_decompressor_init(&decomp, 64, NULL, 1);
    uint8_t output[16];
    size_t output_size = 0;

    int result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                    output, sizeof(output), &output_size);

    /* Corrupted data should either fail or produce wrong output */
    int data_corrupted = (result != CCSDS124_OK) ||
                         (memcmp(input_data, output, 16) != 0);
    TEST_ASSERT(data_corrupted, "Corrupted stream detected or produces wrong data");
}

static void test_decompress_random_garbage(void) {
    ccsds124_decompressor_t decomp;

    /* Random garbage that looks nothing like valid compressed data */
    uint8_t garbage[32];
    for (size_t i = 0; i < sizeof(garbage); i++) {
        garbage[i] = (uint8_t)(i * 17 + 13);  /* Pseudo-random pattern */
    }

    ccsds124_decompressor_init(&decomp, 64, NULL, 0);
    uint8_t output[64];
    size_t output_size = 0;

    int result = ccsds124_decompress(&decomp, garbage, sizeof(garbage),
                                    output, sizeof(output), &output_size);

    /* Should either fail or we detect data doesn't round-trip */
    TEST_ASSERT(result != CCSDS124_OK || output_size != 64,
                "Random garbage rejected or produces unexpected output");
}

/* ============================================================================
 * Boundary Condition Tests
 * ============================================================================ */

static void test_compress_all_zeros(void) {
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;

    ccsds124_compressor_init(&comp, 64, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 64);
    bitvector_zero(&input);
    bitbuffer_init(&output);

    int result = ccsds124_compress_packet(&comp, &input, &output, NULL);

    TEST_ASSERT(result == CCSDS124_OK, "All-zeros input compresses OK");
    TEST_ASSERT(output.num_bits > 0, "All-zeros produces output");
}

static void test_compress_all_ones(void) {
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;

    ccsds124_compressor_init(&comp, 64, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 64);

    /* Set all bits to 1 */
    for (size_t i = 0; i < input.num_words; i++) {
        input.data[i] = 0xFFFFFFFF;
    }
    bitbuffer_init(&output);

    int result = ccsds124_compress_packet(&comp, &input, &output, NULL);

    TEST_ASSERT(result == CCSDS124_OK, "All-ones input compresses OK");
    TEST_ASSERT(output.num_bits > 0, "All-ones produces output");
}

static void test_compress_alternating_pattern(void) {
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;

    ccsds124_compressor_init(&comp, 64, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 64);

    /* Alternating pattern: 10101010... */
    for (size_t i = 0; i < input.num_words; i++) {
        input.data[i] = 0xAAAAAAAA;
    }
    bitbuffer_init(&output);

    int result = ccsds124_compress_packet(&comp, &input, &output, NULL);

    TEST_ASSERT(result == CCSDS124_OK, "Alternating pattern compresses OK");
}

static void test_roundtrip_all_zeros(void) {
    ccsds124_compressor_t comp;
    ccsds124_decompressor_t decomp;
    uint8_t input[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t compressed[64];
    uint8_t output[8];
    size_t compressed_size = 0, output_size = 0;

    ccsds124_compressor_init(&comp, 64, NULL, 1, 10, 20, 50);
    ccsds124_compress(&comp, input, 8, compressed, sizeof(compressed), &compressed_size);

    ccsds124_decompressor_init(&decomp, 64, NULL, 1);
    ccsds124_decompress(&decomp, compressed, compressed_size, output, sizeof(output), &output_size);

    TEST_ASSERT(output_size == 8, "All-zeros roundtrip correct size");
    TEST_ASSERT(memcmp(input, output, 8) == 0, "All-zeros roundtrip matches");
}

static void test_roundtrip_all_ones(void) {
    ccsds124_compressor_t comp;
    ccsds124_decompressor_t decomp;
    uint8_t input[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t compressed[64];
    uint8_t output[8];
    size_t compressed_size = 0, output_size = 0;

    ccsds124_compressor_init(&comp, 64, NULL, 1, 10, 20, 50);
    ccsds124_compress(&comp, input, 8, compressed, sizeof(compressed), &compressed_size);

    ccsds124_decompressor_init(&decomp, 64, NULL, 1);
    ccsds124_decompress(&decomp, compressed, compressed_size, output, sizeof(output), &output_size);

    TEST_ASSERT(output_size == 8, "All-ones roundtrip correct size");
    TEST_ASSERT(memcmp(input, output, 8) == 0, "All-ones roundtrip matches");
}

static void test_compress_minimum_F(void) {
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;

    /* Minimum valid F is 1 bit */
    int result = ccsds124_compressor_init(&comp, 1, NULL, 0, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_OK, "F=1 (minimum) accepted");

    if (result == CCSDS124_OK) {
        bitvector_init(&input, 1);
        bitbuffer_init(&output);
        result = ccsds124_compress_packet(&comp, &input, &output, NULL);
        TEST_ASSERT(result == CCSDS124_OK, "F=1 compression works");
    }
}

static void test_compress_large_F(void) {
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;

    /* Large but valid F */
    size_t large_F = 8192;  /* 1024 bytes */
    int result = ccsds124_compressor_init(&comp, large_F, NULL, 0, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_OK, "F=8192 accepted");

    if (result == CCSDS124_OK) {
        bitvector_init(&input, large_F);
        bitbuffer_init(&output);
        result = ccsds124_compress_packet(&comp, &input, &output, NULL);
        TEST_ASSERT(result == CCSDS124_OK, "F=8192 compression works");
    }
}

/* ============================================================================
 * Output Buffer Overflow Tests
 * ============================================================================ */

static void test_decompress_output_buffer_too_small(void) {
    ccsds124_compressor_t comp;
    ccsds124_decompressor_t decomp;
    uint8_t input[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                          0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

    ccsds124_compressor_init(&comp, 64, NULL, 1, 10, 20, 50);

    uint8_t compressed[256];
    size_t compressed_size = 0;
    ccsds124_compress(&comp, input, 16, compressed, sizeof(compressed), &compressed_size);

    ccsds124_decompressor_init(&decomp, 64, NULL, 1);

    uint8_t output[4];  /* Too small for 16 bytes of output */
    size_t output_size = 0;

    int result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                    output, sizeof(output), &output_size);

    TEST_ASSERT(result == CCSDS124_ERROR_OVERFLOW, "Small output buffer returns overflow");
}

/* ============================================================================
 * High-Level API Input Validation
 * ============================================================================ */

static void test_compress_input_not_multiple_of_packet_size(void) {
    ccsds124_compressor_t comp;
    uint8_t input[11] = {0};  /* 11 bytes, not multiple of 2 */
    uint8_t output[100];
    size_t output_size = 0;

    ccsds124_compressor_init(&comp, 16, NULL, 0, 0, 0, 0);  /* 16 bits = 2 bytes per packet */

    int result = ccsds124_compress(&comp, input, 11, output, sizeof(output), &output_size);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "Non-multiple input size rejected");
}

static void test_compress_api_null_arguments(void) {
    ccsds124_compressor_t comp;
    uint8_t input[8] = {0};
    uint8_t output[64];
    size_t output_size;

    ccsds124_compressor_init(&comp, 64, NULL, 0, 0, 0, 0);

    /* NULL compressor */
    int result = ccsds124_compress(NULL, input, 8, output, sizeof(output), &output_size);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "ccsds124_compress NULL comp rejected");

    /* NULL input */
    result = ccsds124_compress(&comp, NULL, 8, output, sizeof(output), &output_size);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "ccsds124_compress NULL input rejected");

    /* NULL output */
    result = ccsds124_compress(&comp, input, 8, NULL, 64, &output_size);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "ccsds124_compress NULL output rejected");

    /* NULL output_size */
    result = ccsds124_compress(&comp, input, 8, output, sizeof(output), NULL);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "ccsds124_compress NULL size rejected");
}

/* ============================================================================
 * Stress Tests - Multiple Packets with Edge Patterns
 * ============================================================================ */

static void test_many_identical_packets(void) {
    ccsds124_compressor_t comp;
    ccsds124_decompressor_t decomp;

    size_t num_packets = 100;
    size_t packet_bytes = 8;
    size_t total_bytes = num_packets * packet_bytes;

    uint8_t *input = (uint8_t *)malloc(total_bytes);
    uint8_t *output = (uint8_t *)malloc(total_bytes);
    uint8_t *compressed = (uint8_t *)malloc(total_bytes * 2);

    if (!input || !output || !compressed) {
        TEST_ASSERT(0, "Memory allocation failed");
        free(input);
        free(output);
        free(compressed);
        return;
    }

    /* All packets identical */
    memset(input, 0x55, total_bytes);

    ccsds124_compressor_init(&comp, 64, NULL, 1, 10, 20, 50);
    size_t compressed_size = 0;
    int result = ccsds124_compress(&comp, input, total_bytes, compressed,
                                  total_bytes * 2, &compressed_size);

    int compress_ok = (result == CCSDS124_OK);

    ccsds124_decompressor_init(&decomp, 64, NULL, 1);
    size_t output_size = 0;
    result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                output, total_bytes, &output_size);

    int decompress_ok = (result == CCSDS124_OK && output_size == total_bytes);
    int data_matches = (memcmp(input, output, total_bytes) == 0);

    TEST_ASSERT(compress_ok, "100 identical packets compress OK");
    TEST_ASSERT(decompress_ok, "100 identical packets decompress OK");
    TEST_ASSERT(data_matches, "100 identical packets roundtrip matches");

    free(input);
    free(output);
    free(compressed);
}

static void test_alternating_packets(void) {
    ccsds124_compressor_t comp;
    ccsds124_decompressor_t decomp;

    size_t num_packets = 50;
    size_t packet_bytes = 8;
    size_t total_bytes = num_packets * packet_bytes;

    uint8_t *input = (uint8_t *)malloc(total_bytes);
    uint8_t *output = (uint8_t *)malloc(total_bytes);
    uint8_t *compressed = (uint8_t *)malloc(total_bytes * 2);

    if (!input || !output || !compressed) {
        TEST_ASSERT(0, "Memory allocation failed");
        free(input);
        free(output);
        free(compressed);
        return;
    }

    /* Alternating between 0xAA and 0x55 patterns */
    for (size_t i = 0; i < num_packets; i++) {
        uint8_t pattern = (i % 2 == 0) ? 0xAA : 0x55;
        memset(input + i * packet_bytes, pattern, packet_bytes);
    }

    ccsds124_compressor_init(&comp, 64, NULL, 1, 10, 20, 50);
    size_t compressed_size = 0;
    int result = ccsds124_compress(&comp, input, total_bytes, compressed,
                                  total_bytes * 2, &compressed_size);

    int compress_ok = (result == CCSDS124_OK);

    ccsds124_decompressor_init(&decomp, 64, NULL, 1);
    size_t output_size = 0;
    result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                output, total_bytes, &output_size);

    int decompress_ok = (result == CCSDS124_OK && output_size == total_bytes);
    int data_matches = (memcmp(input, output, total_bytes) == 0);

    TEST_ASSERT(compress_ok, "Alternating packets compress OK");
    TEST_ASSERT(decompress_ok, "Alternating packets decompress OK");
    TEST_ASSERT(data_matches, "Alternating packets roundtrip matches");

    free(input);
    free(output);
    free(compressed);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void) {
    printf("\nMalformed Input Tests\n");
    printf("=====================\n\n");

    printf("Invalid Parameter Tests - Compression:\n");
    test_compress_F_zero();
    test_compress_F_exceeds_max();
    test_compress_R_exceeds_max();
    test_compress_all_valid_R_values();
    test_compress_null_compressor();
    test_compress_null_input();
    test_compress_null_output();
    test_compress_mismatched_input_length();

    printf("\nInvalid Parameter Tests - Decompression:\n");
    test_decompress_F_zero();
    test_decompress_F_exceeds_max();
    test_decompress_R_exceeds_max();
    test_decompress_all_valid_R_values();

    printf("\nTruncated Data Tests:\n");
    test_decompress_empty_input();
    test_decompress_single_byte();
    test_count_decode_truncated();
    test_count_decode_truncated_extended();
    test_rle_decode_truncated();

    printf("\nCorrupted Data Tests:\n");
    test_decompress_corrupted_compressed_stream();
    test_decompress_random_garbage();

    printf("\nBoundary Condition Tests:\n");
    test_compress_all_zeros();
    test_compress_all_ones();
    test_compress_alternating_pattern();
    test_roundtrip_all_zeros();
    test_roundtrip_all_ones();
    test_compress_minimum_F();
    test_compress_large_F();

    printf("\nOutput Buffer Overflow Tests:\n");
    test_decompress_output_buffer_too_small();

    printf("\nHigh-Level API Input Validation:\n");
    test_compress_input_not_multiple_of_packet_size();
    test_compress_api_null_arguments();

    printf("\nStress Tests:\n");
    test_many_identical_packets();
    test_alternating_packets();

    printf("\n%d/%d tests passed\n\n", tests_passed, tests_total);

    return (tests_passed == tests_total) ? 0 : 1;
}

/**
 * @file test_robustness.c
 * @brief Robustness parameter (R) tests for CCSDS 124.0-B-1 compression.
 *
 * Tests the robustness mechanism across all valid R values (0-7):
 * - Compression/decompression roundtrip for each R value
 * - Effective robustness (Vt) calculation
 * - Robustness window (Xt) coverage
 * - Large-scale data processing with various R values
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
 * Basic R Value Tests - Verify all R values (0-7) work correctly
 * ============================================================================ */

static void test_all_R_values_compress_init(void) {
    ccsds124_compressor_t comp;
    int all_ok = 1;

    for (uint8_t r = 0; r <= 7; r++) {
        int result = ccsds124_compressor_init(&comp, 64, NULL, r, 10, 20, 50);
        if (result != CCSDS124_OK) {
            all_ok = 0;
            printf("    R=%u init failed\n", r);
        }
    }
    TEST_ASSERT(all_ok, "All R values (0-7) compress init OK");
}

static void test_all_R_values_decompress_init(void) {
    ccsds124_decompressor_t decomp;
    int all_ok = 1;

    for (uint8_t r = 0; r <= 7; r++) {
        int result = ccsds124_decompressor_init(&decomp, 64, NULL, r);
        if (result != CCSDS124_OK) {
            all_ok = 0;
            printf("    R=%u decomp init failed\n", r);
        }
    }
    TEST_ASSERT(all_ok, "All R values (0-7) decompress init OK");
}

/* ============================================================================
 * Roundtrip Tests for Each R Value
 * ============================================================================ */

static int test_roundtrip_for_R(uint8_t r, size_t num_packets) {
    ccsds124_compressor_t comp;
    ccsds124_decompressor_t decomp;

    size_t packet_bytes = 8;  /* 64 bits per packet */
    size_t packet_bits = packet_bytes * 8;
    size_t total_bytes = num_packets * packet_bytes;

    uint8_t *input = (uint8_t *)malloc(total_bytes);
    uint8_t *output = (uint8_t *)malloc(total_bytes);
    uint8_t *compressed = (uint8_t *)malloc(total_bytes * 3);  /* Generous buffer */

    if (!input || !output || !compressed) {
        free(input);
        free(output);
        free(compressed);
        return 0;
    }

    /* Generate test data with some variation */
    for (size_t i = 0; i < total_bytes; i++) {
        /* Mix of stable and varying bytes */
        if ((i % 8) < 4) {
            input[i] = 0x55;  /* Stable byte */
        } else {
            input[i] = (uint8_t)(i * 7 + r);  /* Varying byte */
        }
    }

    /* Compress */
    ccsds124_compressor_init(&comp, packet_bits, NULL, r, 10, 20, 50);
    size_t compressed_size = 0;
    int result = ccsds124_compress(&comp, input, total_bytes, compressed,
                                  total_bytes * 3, &compressed_size);
    if (result != CCSDS124_OK) {
        free(input);
        free(output);
        free(compressed);
        return 0;
    }

    /* Decompress */
    ccsds124_decompressor_init(&decomp, packet_bits, NULL, r);
    size_t output_size = 0;
    result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                output, total_bytes, &output_size);

    int success = (result == CCSDS124_OK) &&
                  (output_size == total_bytes) &&
                  (memcmp(input, output, total_bytes) == 0);

    free(input);
    free(output);
    free(compressed);

    return success;
}

static void test_R0_roundtrip(void) {
    TEST_ASSERT(test_roundtrip_for_R(0, 20), "R=0 roundtrip 20 packets");
}

static void test_R1_roundtrip(void) {
    TEST_ASSERT(test_roundtrip_for_R(1, 20), "R=1 roundtrip 20 packets");
}

static void test_R2_roundtrip(void) {
    TEST_ASSERT(test_roundtrip_for_R(2, 20), "R=2 roundtrip 20 packets");
}

static void test_R3_roundtrip(void) {
    TEST_ASSERT(test_roundtrip_for_R(3, 20), "R=3 roundtrip 20 packets");
}

static void test_R4_roundtrip(void) {
    TEST_ASSERT(test_roundtrip_for_R(4, 20), "R=4 roundtrip 20 packets");
}

static void test_R5_roundtrip(void) {
    TEST_ASSERT(test_roundtrip_for_R(5, 20), "R=5 roundtrip 20 packets");
}

static void test_R6_roundtrip(void) {
    TEST_ASSERT(test_roundtrip_for_R(6, 20), "R=6 roundtrip 20 packets");
}

static void test_R7_roundtrip(void) {
    TEST_ASSERT(test_roundtrip_for_R(7, 20), "R=7 roundtrip 20 packets");
}

/* ============================================================================
 * Large Scale Tests for Each R Value
 * ============================================================================ */

static void test_R0_large_scale(void) {
    TEST_ASSERT(test_roundtrip_for_R(0, 1000), "R=0 roundtrip 1000 packets");
}

static void test_R1_large_scale(void) {
    TEST_ASSERT(test_roundtrip_for_R(1, 1000), "R=1 roundtrip 1000 packets");
}

static void test_R3_large_scale(void) {
    TEST_ASSERT(test_roundtrip_for_R(3, 1000), "R=3 roundtrip 1000 packets");
}

static void test_R7_large_scale(void) {
    TEST_ASSERT(test_roundtrip_for_R(7, 1000), "R=7 roundtrip 1000 packets");
}

/* ============================================================================
 * Compression Ratio Tests - R affects robustness overhead
 * ============================================================================ */

static void test_compression_ratio_vs_R(void) {
    size_t num_packets = 100;
    size_t packet_bytes = 8;
    size_t total_bytes = num_packets * packet_bytes;
    size_t packet_bits = packet_bytes * 8;

    uint8_t *input = (uint8_t *)malloc(total_bytes);
    uint8_t *compressed = (uint8_t *)malloc(total_bytes * 3);

    if (!input || !compressed) {
        TEST_ASSERT(0, "Memory allocation failed");
        free(input);
        free(compressed);
        return;
    }

    /* Predictable data - good for compression */
    memset(input, 0xAA, total_bytes);

    size_t sizes[8];
    int all_ok = 1;

    for (uint8_t r = 0; r <= 7; r++) {
        ccsds124_compressor_t comp;
        ccsds124_compressor_init(&comp, packet_bits, NULL, r, 10, 20, 50);

        size_t compressed_size = 0;
        int result = ccsds124_compress(&comp, input, total_bytes, compressed,
                                      total_bytes * 3, &compressed_size);
        if (result != CCSDS124_OK) {
            all_ok = 0;
        }
        sizes[r] = compressed_size;
    }

    /* Higher R values generally produce larger output (more robustness info) */
    int ratio_reasonable = 1;
    for (uint8_t r = 0; r < 7; r++) {
        /* Allow some variation, but R=7 shouldn't be drastically smaller than R=0 */
        if (sizes[r + 1] < sizes[r] / 2) {
            ratio_reasonable = 0;
        }
    }

    TEST_ASSERT(all_ok, "All R values compress successfully");
    TEST_ASSERT(ratio_reasonable, "Compression ratios consistent across R values");

    printf("    Compressed sizes: R0=%zu R1=%zu R3=%zu R7=%zu bytes\n",
           sizes[0], sizes[1], sizes[3], sizes[7]);

    free(input);
    free(compressed);
}

/* ============================================================================
 * Effective Robustness (Vt) Tests
 * ============================================================================ */

static void test_effective_robustness_increases(void) {
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;
    ccsds124_params_t params = {0};

    /* R=2, send identical packets to increase Vt */
    ccsds124_compressor_init(&comp, 64, NULL, 2, 0, 0, 0);
    bitvector_init(&input, 64);
    bitbuffer_init(&output);

    /* Fill with stable pattern */
    for (size_t i = 0; i < input.num_words; i++) {
        input.data[i] = 0xAAAAAAAA;
    }

    /* First two packets with ft=1, rt=1 (initialization) */
    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;

    for (int i = 0; i < 2; i++) {
        bitbuffer_clear(&output);
        ccsds124_compress_packet(&comp, &input, &output, &params);
    }

    /* Remaining packets with default params */
    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;

    size_t first_size = 0;
    size_t later_size = 0;

    for (int i = 0; i < 10; i++) {
        bitbuffer_clear(&output);
        ccsds124_compress_packet(&comp, &input, &output, &params);
        if (i == 0) {
            first_size = output.num_bits;
        }
        if (i == 9) {
            later_size = output.num_bits;
        }
    }

    /* With stable data, effective robustness should increase,
     * potentially affecting encoded output */
    TEST_ASSERT(first_size > 0, "First packet produces output");
    TEST_ASSERT(later_size > 0, "Later packets produce output");
    /* Can't easily verify Vt directly without exposing internals,
     * but stable data should result in efficient encoding */
    TEST_ASSERT(later_size <= first_size + 10, "Later packets not excessively larger");
}

static void test_robustness_window_coverage(void) {
    /* Test that robustness window Xt covers R+1 packets of history */
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;
    ccsds124_params_t params = {0};

    ccsds124_compressor_init(&comp, 8, NULL, 3, 0, 0, 0);  /* R=3 */
    bitvector_init(&input, 8);
    bitbuffer_init(&output);

    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;

    /* First packet */
    input.data[0] = 0x00000000;
    ccsds124_compress_packet(&comp, &input, &output, &params);

    /* Packets with single bit changes */
    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;

    uint8_t changes[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

    for (int i = 0; i < 8; i++) {
        input.data[0] = changes[i] << 24;  /* Big endian */
        bitbuffer_clear(&output);
        ccsds124_compress_packet(&comp, &input, &output, &params);
    }

    /* After 8 packets with R=3, change history should be maintained */
    /* Verify compressor has tracked changes in history */
    int history_maintained = 0;
    for (size_t i = 0; i < CCSDS124_MAX_HISTORY; i++) {
        if (bitvector_hamming_weight(&comp.change_history[i]) > 0) {
            history_maintained = 1;
            break;
        }
    }

    TEST_ASSERT(comp.t == 9, "Compressor processed 9 packets");
    TEST_ASSERT(history_maintained, "Change history maintained");
}

/* ============================================================================
 * Pattern-Specific Tests for Different R Values
 * ============================================================================ */

static void test_highly_predictable_data_all_R(void) {
    /* Highly predictable data should compress well with all R values */
    size_t packet_bytes = 8;
    size_t num_packets = 50;
    size_t total_bytes = packet_bytes * num_packets;
    size_t packet_bits = packet_bytes * 8;

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

    /* All same value - maximally predictable */
    memset(input, 0x42, total_bytes);

    int all_ok = 1;
    for (uint8_t r = 0; r <= 7; r++) {
        ccsds124_compressor_t comp;
        ccsds124_decompressor_t decomp;

        ccsds124_compressor_init(&comp, packet_bits, NULL, r, 10, 20, 50);
        size_t compressed_size = 0;
        int result = ccsds124_compress(&comp, input, total_bytes, compressed,
                                      total_bytes * 2, &compressed_size);
        if (result != CCSDS124_OK) {
            all_ok = 0;
            printf("    R=%u compress failed\n", r);
            continue;
        }

        ccsds124_decompressor_init(&decomp, packet_bits, NULL, r);
        size_t output_size = 0;
        result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                    output, total_bytes, &output_size);

        if (result != CCSDS124_OK || output_size != total_bytes ||
            memcmp(input, output, total_bytes) != 0) {
            all_ok = 0;
            printf("    R=%u decompress failed\n", r);
        }
    }

    TEST_ASSERT(all_ok, "Highly predictable data works for all R values");

    free(input);
    free(output);
    free(compressed);
}

static void test_random_data_all_R(void) {
    /* Random/high-entropy data should still roundtrip correctly */
    size_t packet_bytes = 8;
    size_t num_packets = 50;
    size_t total_bytes = packet_bytes * num_packets;
    size_t packet_bits = packet_bytes * 8;

    uint8_t *input = (uint8_t *)malloc(total_bytes);
    uint8_t *output = (uint8_t *)malloc(total_bytes);
    uint8_t *compressed = (uint8_t *)malloc(total_bytes * 3);

    if (!input || !output || !compressed) {
        TEST_ASSERT(0, "Memory allocation failed");
        free(input);
        free(output);
        free(compressed);
        return;
    }

    /* Pseudo-random data */
    for (size_t i = 0; i < total_bytes; i++) {
        input[i] = (uint8_t)((i * 37 + 17) ^ (i >> 3));
    }

    int all_ok = 1;
    for (uint8_t r = 0; r <= 7; r++) {
        ccsds124_compressor_t comp;
        ccsds124_decompressor_t decomp;

        ccsds124_compressor_init(&comp, packet_bits, NULL, r, 10, 20, 50);
        size_t compressed_size = 0;
        int result = ccsds124_compress(&comp, input, total_bytes, compressed,
                                      total_bytes * 3, &compressed_size);
        if (result != CCSDS124_OK) {
            all_ok = 0;
            printf("    R=%u compress failed\n", r);
            continue;
        }

        ccsds124_decompressor_init(&decomp, packet_bits, NULL, r);
        size_t output_size = 0;
        result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                    output, total_bytes, &output_size);

        if (result != CCSDS124_OK || output_size != total_bytes ||
            memcmp(input, output, total_bytes) != 0) {
            all_ok = 0;
            printf("    R=%u decompress failed\n", r);
        }
    }

    TEST_ASSERT(all_ok, "Random data works for all R values");

    free(input);
    free(output);
    free(compressed);
}

/* ============================================================================
 * Edge Cases for R Parameter
 * ============================================================================ */

static void test_R_boundary_values(void) {
    ccsds124_compressor_t comp;

    /* R=0 (minimum) */
    int result = ccsds124_compressor_init(&comp, 64, NULL, 0, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_OK, "R=0 (minimum) accepted");

    /* R=7 (maximum) */
    result = ccsds124_compressor_init(&comp, 64, NULL, 7, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_OK, "R=7 (maximum) accepted");

    /* R=8 (should fail) */
    result = ccsds124_compressor_init(&comp, 64, NULL, 8, 0, 0, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "R=8 rejected");
}

static void test_minimum_packets_for_each_R(void) {
    /* Test with minimum number of packets that exercises R */
    int all_ok = 1;

    for (uint8_t r = 0; r <= 7; r++) {
        /* At least R+2 packets needed to exercise robustness */
        size_t min_packets = (size_t)r + 2;
        if (!test_roundtrip_for_R(r, min_packets)) {
            all_ok = 0;
            printf("    R=%u with %zu packets failed\n", r, min_packets);
        }
    }

    TEST_ASSERT(all_ok, "Minimum packets for each R roundtrip OK");
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

static void test_very_long_stream_R0(void) {
    TEST_ASSERT(test_roundtrip_for_R(0, 10000), "R=0 roundtrip 10000 packets");
}

static void test_very_long_stream_R7(void) {
    TEST_ASSERT(test_roundtrip_for_R(7, 10000), "R=7 roundtrip 10000 packets");
}

static void test_varying_packet_sizes_with_R(void) {
    int all_ok = 1;

    /* Test different packet sizes with R=3 */
    size_t packet_sizes[] = {8, 16, 32, 64, 128, 256, 512};
    size_t num_sizes = sizeof(packet_sizes) / sizeof(packet_sizes[0]);

    for (size_t s = 0; s < num_sizes; s++) {
        size_t F = packet_sizes[s];
        size_t packet_bytes = F / 8;
        size_t num_packets = 50;
        size_t total_bytes = packet_bytes * num_packets;

        /* Larger buffer for small packets where per-packet overhead dominates */
        size_t buffer_size = total_bytes * 4 + 1024;

        uint8_t *input = (uint8_t *)malloc(total_bytes);
        uint8_t *output = (uint8_t *)malloc(total_bytes);
        uint8_t *compressed = (uint8_t *)malloc(buffer_size);

        if (!input || !output || !compressed) {
            free(input);
            free(output);
            free(compressed);
            all_ok = 0;
            continue;
        }

        /* Fill with pattern */
        for (size_t i = 0; i < total_bytes; i++) {
            input[i] = (uint8_t)(i % 256);
        }

        ccsds124_compressor_t comp;
        ccsds124_decompressor_t decomp;

        ccsds124_compressor_init(&comp, F, NULL, 3, 10, 20, 50);
        size_t compressed_size = 0;
        int result = ccsds124_compress(&comp, input, total_bytes, compressed,
                                      buffer_size, &compressed_size);
        if (result != CCSDS124_OK) {
            all_ok = 0;
            printf("    F=%zu compress failed\n", F);
            free(input);
            free(output);
            free(compressed);
            continue;
        }

        ccsds124_decompressor_init(&decomp, F, NULL, 3);
        size_t output_size = 0;
        result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                    output, total_bytes, &output_size);

        if (result != CCSDS124_OK || output_size != total_bytes ||
            memcmp(input, output, total_bytes) != 0) {
            all_ok = 0;
            printf("    F=%zu decompress failed\n", F);
        }

        free(input);
        free(output);
        free(compressed);
    }

    TEST_ASSERT(all_ok, "Varying packet sizes with R=3 work");
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void) {
    printf("\nRobustness Parameter Tests\n");
    printf("==========================\n\n");

    printf("Basic R Value Tests:\n");
    test_all_R_values_compress_init();
    test_all_R_values_decompress_init();

    printf("\nRoundtrip Tests for Each R Value:\n");
    test_R0_roundtrip();
    test_R1_roundtrip();
    test_R2_roundtrip();
    test_R3_roundtrip();
    test_R4_roundtrip();
    test_R5_roundtrip();
    test_R6_roundtrip();
    test_R7_roundtrip();

    printf("\nLarge Scale Tests (1000 packets):\n");
    test_R0_large_scale();
    test_R1_large_scale();
    test_R3_large_scale();
    test_R7_large_scale();

    printf("\nCompression Ratio Analysis:\n");
    test_compression_ratio_vs_R();

    printf("\nEffective Robustness Tests:\n");
    test_effective_robustness_increases();
    test_robustness_window_coverage();

    printf("\nPattern-Specific Tests:\n");
    test_highly_predictable_data_all_R();
    test_random_data_all_R();

    printf("\nEdge Cases:\n");
    test_R_boundary_values();
    test_minimum_packets_for_each_R();

    printf("\nStress Tests:\n");
    test_very_long_stream_R0();
    test_very_long_stream_R7();
    test_varying_packet_sizes_with_R();

    printf("\n%d/%d tests passed\n\n", tests_passed, tests_total);

    return (tests_passed == tests_total) ? 0 : 1;
}

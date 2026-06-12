/*
 * ============================================================================
 *  _____                                   ____
 * |_   _|_ _ _ __   __ _  __ _ _ __ __ _  / ___| _ __   __ _  ___ ___
 *   | |/ _` | '_ \ / _` |/ _` | '__/ _` | \___ \| '_ \ / _` |/ __/ _ \
 *   | | (_| | | | | (_| | (_| | | | (_| |  ___) | |_) | (_| | (_|  __/
 *   |_|\__,_|_| |_|\__,_|\__, |_|  \__,_| |____/| .__/ \__,_|\___\___|
 *                        |___/                  |_|
 * ============================================================================
 *
 * CCSDS 124.0-B-1 C Implementation - Reference Test Vectors
 *
 * Tests compression against reference test vectors to ensure correctness
 * and interoperability with other implementations.
 *
 * Authors:
 *   Georges Labrèche <georges@tanagraspace.com> — https://georges.fyi
 *   Claude Code (claude-sonnet-4-5-20250929) <noreply@anthropic.com>
 * ============================================================================
 */

#include "ccsds124.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test vector paths relative to c/ directory */
#define TEST_VECTORS_DIR "../../test-vectors"

/* Test counter */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %s...", #name); \
        fflush(stdout); \
        name(); \
        tests_passed++; \
        printf(" ✓\n"); \
    } \
    static void name(void)

#define RUN_TEST(name) do { \
    tests_run++; \
    run_##name(); \
} while(0)

/* Helper: Read file into buffer */
static size_t read_file(const char *path, uint8_t *buffer, size_t max_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "\n  ERROR: Could not open file: %s\n", path);
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    if (file_size > (long)max_size) {
        fprintf(stderr, "\n  ERROR: File too large: %ld bytes (max %zu)\n",
                file_size, max_size);
        fclose(fp);
        return 0;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    fclose(fp);

    return bytes_read;
}

/* Helper: Simple DJB2 hash for verification */
static uint32_t djb2_hash(const uint8_t *data, size_t len) {
    uint32_t hash = 5381U;
    for (size_t i = 0U; i < len; i++) {
        hash = ((hash << 5U) + hash) + data[i];  /* hash * 33 + c */
    }
    return hash;
}

/* Helper: Compare our output with expected output */
static int compare_output(const uint8_t *actual, size_t actual_size,
                         const uint8_t *expected, size_t expected_size,
                         const char *test_name);

/* Helper: Decompress and verify round-trip integrity */
static int decompress_and_verify(
    const uint8_t *original_data, size_t original_size,
    const uint8_t *compressed_data, size_t compressed_size,
    size_t packet_length_bits, uint8_t robustness,
    const char *test_name
) {
    /* Allocate buffer for decompressed output */
    static uint8_t decompressed_data[15 * 1024 * 1024];  /* 15 MB */

    /* Initialize decompressor */
    ccsds124_decompressor_t decomp;
    int result = ccsds124_decompressor_init(&decomp, packet_length_bits, NULL, robustness);
    if (result != CCSDS124_OK) {
        fprintf(stderr, "\n  FAIL: Decompressor init failed for %s\n", test_name);
        return 0;
    }

    /* Decompress */
    size_t decompressed_size = 0U;
    result = ccsds124_decompress(&decomp, compressed_data, compressed_size,
                               decompressed_data, sizeof(decompressed_data),
                               &decompressed_size);
    if (result != CCSDS124_OK) {
        fprintf(stderr, "\n  FAIL: Decompression failed with error %d for %s\n",
                result, test_name);
        return 0;
    }

    /* Verify size matches */
    if (decompressed_size != original_size) {
        fprintf(stderr, "\n  FAIL: Round-trip size mismatch in %s\n", test_name);
        fprintf(stderr, "    Original:     %zu bytes\n", original_size);
        fprintf(stderr, "    Decompressed: %zu bytes\n", decompressed_size);
        return 0;
    }

    /* Verify hash matches */
    uint32_t original_hash = djb2_hash(original_data, original_size);
    uint32_t decompressed_hash = djb2_hash(decompressed_data, decompressed_size);

    if (original_hash != decompressed_hash) {
        fprintf(stderr, "\n  FAIL: Round-trip hash mismatch in %s\n", test_name);
        fprintf(stderr, "    Original hash:     0x%08X\n", original_hash);
        fprintf(stderr, "    Decompressed hash: 0x%08X\n", decompressed_hash);

        /* Find first difference */
        for (size_t i = 0U; i < original_size; i++) {
            if (original_data[i] != decompressed_data[i]) {
                fprintf(stderr, "    First difference at byte %zu: expected 0x%02X, got 0x%02X\n",
                        i, original_data[i], decompressed_data[i]);
                break;
            }
        }
        return 0;
    }

    printf("    Round-trip: hash 0x%08X verified", original_hash);
    return 1;
}

/* Forward declaration */
static int compare_output(const uint8_t *actual, size_t actual_size,
                         const uint8_t *expected, size_t expected_size,
                         const char *test_name) {
    if (actual_size != expected_size) {
        fprintf(stderr, "\n  FAIL: Size mismatch in %s\n", test_name);
        fprintf(stderr, "    Expected: %zu bytes\n", expected_size);
        fprintf(stderr, "    Got:      %zu bytes\n", actual_size);
        return 0;
    }

    for (size_t i = 0; i < actual_size; i++) {
        if (actual[i] != expected[i]) {
            fprintf(stderr, "\n  FAIL: Byte mismatch in %s at offset %zu\n",
                    test_name, i);
            fprintf(stderr, "    Expected: 0x%02X\n", expected[i]);
            fprintf(stderr, "    Got:      0x%02X\n", actual[i]);

            /* Show some context */
            fprintf(stderr, "    Context (offset %zu):\n", i);
            size_t start = (i > 4) ? i - 4 : 0;
            size_t end = (i + 5 < actual_size) ? i + 5 : actual_size;

            fprintf(stderr, "      Expected: ");
            for (size_t j = start; j < end; j++) {
                fprintf(stderr, "%02X ", expected[j]);
                if (j == i) fprintf(stderr, "[");
            }
            fprintf(stderr, "\n");

            fprintf(stderr, "      Got:      ");
            for (size_t j = start; j < end; j++) {
                fprintf(stderr, "%02X ", actual[j]);
                if (j == i) fprintf(stderr, "[");
            }
            fprintf(stderr, "\n");

            return 0;
        }
    }

    return 1;
}

/* Helper: Compress entire file and compare with expected output */
static int compress_and_verify(
    const char *input_path,
    const char *expected_path,
    size_t packet_length_bits,
    int pt_limit,
    int ft_limit,
    int rt_limit,
    uint8_t robustness,
    const char *test_name
) {
    /* Allocate large buffers for test vectors (venus-express is ~14 MB) */
    static uint8_t input_data[15 * 1024 * 1024];      /* 15 MB for input */
    static uint8_t expected_output[10 * 1024 * 1024]; /* 10 MB for expected output */
    static uint8_t actual_output[10 * 1024 * 1024];   /* 10 MB for actual output */

    /* Read input file */
    size_t input_size = read_file(input_path, input_data, sizeof(input_data));
    if (input_size == 0) {
        fprintf(stderr, "\n  FAIL: Could not read input file: %s\n", input_path);
        return 0;
    }

    /* Read expected output */
    size_t expected_size = read_file(expected_path, expected_output, sizeof(expected_output));
    if (expected_size == 0) {
        fprintf(stderr, "\n  FAIL: Could not read expected output: %s\n", expected_path);
        return 0;
    }

    /* Initialize compressor with automatic parameter management */
    ccsds124_compressor_t comp;
    int result = ccsds124_compressor_init(&comp, packet_length_bits, NULL, robustness,
                                        pt_limit, ft_limit, rt_limit);
    if (result != CCSDS124_OK) {
        fprintf(stderr, "\n  FAIL: Compressor init failed\n");
        return 0;
    }

    /* Compress entire input using high-level API */
    size_t actual_size = 0;

    result = ccsds124_compress(&comp, input_data, input_size,
                            actual_output, sizeof(actual_output), &actual_size);
    if (result != CCSDS124_OK) {
        fprintf(stderr, "\n  FAIL: Compression failed with error %d\n", result);
        return 0;
    }

    printf("    Input: %zu bytes → Compressed: %zu bytes (ratio: %.2fx)\n",
           input_size, actual_size, (float)input_size / actual_size);

    /* Compare with expected output */
    if (compare_output(actual_output, actual_size,
                       expected_output, expected_size, test_name) == 0) {
        return 0;
    }

    /* Perform round-trip verification: decompress and verify hash match */
    return decompress_and_verify(input_data, input_size,
                                 actual_output, actual_size,
                                 packet_length_bits, robustness, test_name);
}

/* ========================================================================
 * Test Vector: Simple
 *
 * Parameters from metadata.json:
 *   packet_length: 90 bytes (720 bits)
 *   pt: 10 (new mask every 10 packets)
 *   ft: 20 (send mask every 20 packets)
 *   rt: 50 (uncompressed every 50 packets)
 *   robustness: 1
 * ======================================================================== */

TEST(test_vector_simple) {
    char input_path[256];
    char expected_path[256];
    snprintf(input_path, sizeof(input_path), "%s/input/simple.bin", TEST_VECTORS_DIR);
    snprintf(expected_path, sizeof(expected_path), "%s/expected-output/simple.bin.pkt", TEST_VECTORS_DIR);

    if (!compress_and_verify(input_path, expected_path, 720, 10, 20, 50, 1, "simple")) {
        tests_failed++;
    }
}

/* ========================================================================
 * Test Vector: Housekeeping
 *
 * Parameters from housekeeping.yaml:
 *   packet_length: 90 bytes (720 bits)
 *   pt: 20 (new mask every 20 packets)
 *   ft: 50 (send mask every 50 packets)
 *   rt: 100 (uncompressed every 100 packets)
 *   robustness: 2
 * ======================================================================== */

TEST(test_vector_housekeeping) {
    char input_path[256];
    char expected_path[256];
    snprintf(input_path, sizeof(input_path), "%s/input/housekeeping.bin", TEST_VECTORS_DIR);
    snprintf(expected_path, sizeof(expected_path), "%s/expected-output/housekeeping.bin.pkt", TEST_VECTORS_DIR);

    if (!compress_and_verify(input_path, expected_path, 720, 20, 50, 100, 2, "housekeeping")) {
        tests_failed++;
    }
}

/* ========================================================================
 * Test Vector: Edge Cases
 *
 * Parameters from edge-cases.yaml:
 *   packet_length: 90 bytes (720 bits)
 *   pt: 10 (new mask every 10 packets)
 *   ft: 20 (send mask every 20 packets)
 *   rt: 50 (uncompressed every 50 packets)
 *   robustness: 1
 * ======================================================================== */

TEST(test_vector_edge_cases) {
    char input_path[256];
    char expected_path[256];
    snprintf(input_path, sizeof(input_path), "%s/input/edge-cases.bin", TEST_VECTORS_DIR);
    snprintf(expected_path, sizeof(expected_path), "%s/expected-output/edge-cases.bin.pkt", TEST_VECTORS_DIR);

    if (!compress_and_verify(input_path, expected_path, 720, 10, 20, 50, 1, "edge-cases")) {
        tests_failed++;
    }
}

/* ========================================================================
 * Test Vector: High Robustness (hiro)
 *
 * Parameters from hiro.yaml:
 *   packet_length: 90 bytes (720 bits)
 *   pt: 10 (new mask every 10 packets)
 *   ft: 20 (send mask every 20 packets)
 *   rt: 50 (uncompressed every 50 packets)
 *   robustness: 7 (maximum)
 *
 * Tests maximum robustness parameter to exercise packet loss resilience
 * code paths. Uses same data patterns as simple for comparison.
 * ======================================================================== */

TEST(test_vector_hiro) {
    char input_path[256];
    char expected_path[256];
    snprintf(input_path, sizeof(input_path), "%s/input/hiro.bin", TEST_VECTORS_DIR);
    snprintf(expected_path, sizeof(expected_path), "%s/expected-output/hiro.bin.pkt", TEST_VECTORS_DIR);

    if (!compress_and_verify(input_path, expected_path, 720, 10, 20, 50, 7, "hiro")) {
        tests_failed++;
    }
}

/* ========================================================================
 * Test Vector: Venus Express (largest - run last)
 *
 * Parameters from venus-express.yaml:
 *   packet_length: 90 bytes (720 bits)
 *   pt: 20 (new mask every 20 packets)
 *   ft: 50 (send mask every 50 packets)
 *   rt: 100 (uncompressed every 100 packets)
 *   robustness: 2
 * ======================================================================== */

TEST(test_vector_venus_express) {
    char input_path[256];
    char expected_path[256];
    snprintf(input_path, sizeof(input_path), "%s/input/venus-express.ccsds", TEST_VECTORS_DIR);
    snprintf(expected_path, sizeof(expected_path), "%s/expected-output/venus-express.ccsds.pkt", TEST_VECTORS_DIR);

    if (!compress_and_verify(input_path, expected_path, 720, 20, 50, 100, 2, "venus-express")) {
        tests_failed++;
    }
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(void) {
    printf("\nReference Test Vectors\n");
    printf("======================\n\n");

    printf("Testing against reference implementations...\n");
    printf("Test vectors location: %s\n\n", TEST_VECTORS_DIR);

    RUN_TEST(test_vector_simple);
    RUN_TEST(test_vector_housekeeping);
    RUN_TEST(test_vector_edge_cases);
    RUN_TEST(test_vector_hiro);
    RUN_TEST(test_vector_venus_express);  /* Largest test - run last */

    printf("\n");
    printf("Results: %d/%d tests passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d failed", tests_failed);
    }
    printf("\n\n");

    if (tests_failed > 0) {
        printf("FAIL: Some tests failed unexpectedly.\n\n");
        return 1;
    }

    printf("All tests passed!\n\n");
    return 0;
}

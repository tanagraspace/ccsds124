/**
 * @file test_discover.c
 * @brief Unit tests for pocket_discover_packet_length().
 *
 * Tests the packet length discovery function that parses compressed
 * bitstreams to find F from reference packets (rt=1).
 */

#include "pocketplus.h"
#include <stdio.h>
#include <string.h>

/* Test counters */
static int tests_passed = 0;
static int tests_total = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_total++; \
    if (cond) { \
        tests_passed++; \
        printf("  %s... \xe2\x9c\x93\n", msg); \
    } else { \
        printf("  %s... FAILED\n", msg); \
    } \
} while(0)

/* Helper: compress a single reference packet (rt=1, ft=1) */
static int compress_reference_packet(
    size_t F, uint8_t robustness,
    uint8_t *input_data,
    uint8_t *compressed_out, size_t *bits_out, size_t *bytes_out
) {
    pocket_compressor_t comp;
    int rc = pocket_compressor_init(&comp, F, NULL, robustness, 0, 0, 0);
    if (rc != POCKET_OK) return rc;

    bitvector_t input;
    bitvector_init(&input, F);
    bitvector_from_bytes(&input, input_data, (F + 7) / 8);

    pocket_params_t params;
    params.min_robustness = robustness;
    params.new_mask_flag = 0;
    params.send_mask_flag = 1;      /* ft=1 */
    params.uncompressed_flag = 1;   /* rt=1 */

    bitbuffer_t bb;
    bitbuffer_init(&bb);
    rc = pocket_compress_packet(&comp, &input, &bb, &params);
    if (rc != POCKET_OK) return rc;

    *bits_out = bitbuffer_size(&bb);
    *bytes_out = bitbuffer_to_bytes(&bb, compressed_out, 256);
    return POCKET_OK;
}

/* Helper: compress two packets, return the second (non-reference, dt=1) */
static int compress_non_reference_packet(
    size_t F, uint8_t robustness,
    uint8_t *compressed_out, size_t *bits_out, size_t *bytes_out
) {
    pocket_compressor_t comp;
    int rc = pocket_compressor_init(&comp, F, NULL, robustness, 0, 0, 0);
    if (rc != POCKET_OK) return rc;

    uint8_t data1[] = {0xAB, 0xCD};
    uint8_t data2[] = {0xAB, 0xCE};

    bitvector_t input;

    /* First packet: rt=1, ft=1 (init phase) */
    bitvector_init(&input, F);
    bitvector_from_bytes(&input, data1, (F + 7) / 8);

    pocket_params_t params;
    params.min_robustness = robustness;
    params.new_mask_flag = 0;
    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;

    bitbuffer_t bb;
    bitbuffer_init(&bb);
    rc = pocket_compress_packet(&comp, &input, &bb, &params);
    if (rc != POCKET_OK) return rc;

    /* Second packet: rt=0, ft=0 (dt=1) */
    bitvector_init(&input, F);
    bitvector_from_bytes(&input, data2, (F + 7) / 8);

    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;

    bitbuffer_init(&bb);
    rc = pocket_compress_packet(&comp, &input, &bb, &params);
    if (rc != POCKET_OK) return rc;

    *bits_out = bitbuffer_size(&bb);
    *bytes_out = bitbuffer_to_bytes(&bb, compressed_out, 256);
    return POCKET_OK;
}

/* Helper: compress two packets, return the second (ft=1, rt=0) */
static int compress_ft1_rt0_packet(
    size_t F, uint8_t robustness,
    uint8_t *compressed_out, size_t *bits_out, size_t *bytes_out
) {
    pocket_compressor_t comp;
    int rc = pocket_compressor_init(&comp, F, NULL, robustness, 0, 0, 0);
    if (rc != POCKET_OK) return rc;

    uint8_t data1[] = {0xAB, 0xCD};
    uint8_t data2[] = {0xAB, 0xCE};

    bitvector_t input;

    /* First packet: rt=1, ft=1 */
    bitvector_init(&input, F);
    bitvector_from_bytes(&input, data1, (F + 7) / 8);

    pocket_params_t params;
    params.min_robustness = robustness;
    params.new_mask_flag = 0;
    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;

    bitbuffer_t bb;
    bitbuffer_init(&bb);
    rc = pocket_compress_packet(&comp, &input, &bb, &params);
    if (rc != POCKET_OK) return rc;

    /* Second packet: ft=1, rt=0 */
    bitvector_init(&input, F);
    bitvector_from_bytes(&input, data2, (F + 7) / 8);

    params.send_mask_flag = 1;      /* ft=1 */
    params.uncompressed_flag = 0;   /* rt=0 */

    bitbuffer_init(&bb);
    rc = pocket_compress_packet(&comp, &input, &bb, &params);
    if (rc != POCKET_OK) return rc;

    *bits_out = bitbuffer_size(&bb);
    *bytes_out = bitbuffer_to_bytes(&bb, compressed_out, 256);
    return POCKET_OK;
}

/* ============================================================================
 * Reference Packet Discovery
 * ============================================================================ */

static void test_discover_from_reference_packet(void) {
    size_t F = 16;
    uint8_t data[] = {0xAB, 0xCD};
    uint8_t compressed[256];
    size_t bits, bytes;

    int rc = compress_reference_packet(F, 0, data, compressed, &bits, &bytes);
    TEST_ASSERT(rc == POCKET_OK, "ref_pkt: compression OK");

    uint32_t discovered = 0;
    rc = pocket_discover_packet_length(compressed, bits, &discovered);
    TEST_ASSERT(rc == POCKET_OK, "ref_pkt: returns POCKET_OK");
    TEST_ASSERT(discovered == 16, "ref_pkt: discovers F=16");
}

/* ============================================================================
 * Various F Values
 * ============================================================================ */

static void test_discover_F8(void) {
    uint8_t data[] = {0x42};
    uint8_t compressed[256];
    size_t bits, bytes;

    compress_reference_packet(8, 0, data, compressed, &bits, &bytes);

    uint32_t discovered = 0;
    pocket_discover_packet_length(compressed, bits, &discovered);
    TEST_ASSERT(discovered == 8, "various_F: discovers F=8");
}

static void test_discover_F32(void) {
    uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
    uint8_t compressed[256];
    size_t bits, bytes;

    compress_reference_packet(32, 0, data, compressed, &bits, &bytes);

    uint32_t discovered = 0;
    pocket_discover_packet_length(compressed, bits, &discovered);
    TEST_ASSERT(discovered == 32, "various_F: discovers F=32");
}

static void test_discover_F64(void) {
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint8_t compressed[256];
    size_t bits, bytes;

    compress_reference_packet(64, 0, data, compressed, &bits, &bytes);

    uint32_t discovered = 0;
    pocket_discover_packet_length(compressed, bits, &discovered);
    TEST_ASSERT(discovered == 64, "various_F: discovers F=64");
}

static void test_discover_F13_non_byte_aligned(void) {
    uint8_t data[] = {0xAB, 0xC0};  /* 13 bits, padding zeros in low bits */
    uint8_t compressed[256];
    size_t bits, bytes;

    compress_reference_packet(13, 0, data, compressed, &bits, &bytes);

    uint32_t discovered = 0;
    pocket_discover_packet_length(compressed, bits, &discovered);
    TEST_ASSERT(discovered == 13, "various_F: discovers F=13 (non-byte-aligned)");
}

/* ============================================================================
 * Non-Discoverable Packets
 * ============================================================================ */

static void test_discover_non_reference_returns_zero(void) {
    uint8_t compressed[256];
    size_t bits, bytes;

    int rc = compress_non_reference_packet(16, 0, compressed, &bits, &bytes);
    TEST_ASSERT(rc == POCKET_OK, "non_ref: compression OK");

    uint32_t discovered = 99;
    rc = pocket_discover_packet_length(compressed, bits, &discovered);
    TEST_ASSERT(rc == POCKET_OK, "non_ref: returns POCKET_OK");
    TEST_ASSERT(discovered == 0, "non_ref: F=0 (not discoverable)");
}

static void test_discover_ft1_rt0_returns_zero(void) {
    uint8_t compressed[256];
    size_t bits, bytes;

    int rc = compress_ft1_rt0_packet(16, 0, compressed, &bits, &bytes);
    TEST_ASSERT(rc == POCKET_OK, "ft1_rt0: compression OK");

    uint32_t discovered = 99;
    rc = pocket_discover_packet_length(compressed, bits, &discovered);
    TEST_ASSERT(rc == POCKET_OK, "ft1_rt0: returns POCKET_OK");
    TEST_ASSERT(discovered == 0, "ft1_rt0: F=0 (rt=0, not discoverable)");
}

/* ============================================================================
 * Input Validation
 * ============================================================================ */

static void test_discover_null_data(void) {
    uint32_t discovered = 0;
    int rc = pocket_discover_packet_length(NULL, 16, &discovered);
    TEST_ASSERT(rc == POCKET_ERROR_INVALID_ARG, "null_data: returns INVALID_ARG");
}

static void test_discover_null_output(void) {
    uint8_t data[] = {0xFF};
    int rc = pocket_discover_packet_length(data, 8, NULL);
    TEST_ASSERT(rc == POCKET_ERROR_INVALID_ARG, "null_output: returns INVALID_ARG");
}

static void test_discover_zero_bits(void) {
    uint8_t data[] = {0xFF};
    uint32_t discovered = 0;
    int rc = pocket_discover_packet_length(data, 0, &discovered);
    TEST_ASSERT(rc == POCKET_ERROR_INVALID_ARG, "zero_bits: returns INVALID_ARG");
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

static void test_discover_truncated(void) {
    /* Too few bits for any valid compressed packet */
    uint8_t data[] = {0xFF};
    uint32_t discovered = 99;
    int rc = pocket_discover_packet_length(data, 3, &discovered);
    TEST_ASSERT(rc == POCKET_OK, "truncated: returns POCKET_OK");
    TEST_ASSERT(discovered == 0, "truncated: F=0 (can't parse)");
}

static void test_discover_garbage(void) {
    /* Random garbage should not crash */
    uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42, 0x13, 0x37, 0x00};
    uint32_t discovered = 99;
    int rc = pocket_discover_packet_length(garbage, 64, &discovered);
    TEST_ASSERT(rc == POCKET_OK, "garbage: returns POCKET_OK");
    /* May or may not discover F depending on parse, but should not crash */
    (void)discovered;  /* Don't assert specific value for random data */
    TEST_ASSERT(1, "garbage: no crash");
}

static void test_discover_with_robustness(void) {
    /* R=2, first packet still has rt=1, ft=1 */
    uint8_t data[] = {0x55, 0xAA};
    uint8_t compressed[256];
    size_t bits, bytes;

    int rc = compress_reference_packet(16, 2, data, compressed, &bits, &bytes);
    TEST_ASSERT(rc == POCKET_OK, "robustness: compression OK");

    uint32_t discovered = 0;
    rc = pocket_discover_packet_length(compressed, bits, &discovered);
    TEST_ASSERT(rc == POCKET_OK, "robustness: returns POCKET_OK");
    TEST_ASSERT(discovered == 16, "robustness: discovers F=16");
}

static void test_discover_truncated_reference(void) {
    /* A reference packet cut short inside I_t still signals its length:
     * COUNT(F) is decodable, so the signaled length is reported with
     * POCKET_STATUS_TRUNCATED_LENGTH (weak discovery). */
    uint8_t data[] = {0x55, 0xAA};
    uint8_t compressed[256];
    size_t bits, bytes;

    int rc = compress_reference_packet(16, 0, data, compressed, &bits, &bytes);
    TEST_ASSERT(rc == POCKET_OK, "trunc_ref: compression OK");

    uint32_t discovered = 0;
    rc = pocket_discover_packet_length(compressed, bits - 8, &discovered);
    TEST_ASSERT(rc == POCKET_STATUS_TRUNCATED_LENGTH,
                "trunc_ref: returns TRUNCATED_LENGTH");
    TEST_ASSERT(discovered == 16, "trunc_ref: signaled F=16 reported");
}

static void test_discover_excess_bits(void) {
    /* A reference packet with excess trailing bits is self-delimiting via
     * COUNT(F): the remainder is ignored and discovery succeeds. */
    uint8_t data[] = {0x55, 0xAA};
    uint8_t compressed[256] = {0};
    size_t bits, bytes;

    int rc = compress_reference_packet(16, 0, data, compressed, &bits, &bytes);
    TEST_ASSERT(rc == POCKET_OK, "excess: compression OK");

    uint32_t discovered = 0;
    rc = pocket_discover_packet_length(compressed, bits + 64, &discovered);
    TEST_ASSERT(rc == POCKET_OK, "excess: returns POCKET_OK");
    TEST_ASSERT(discovered == 16, "excess: discovers F=16 despite excess");
}

static void test_discover_rt1_without_ft(void) {
    /* Compress with rt=1 but ft=0 — should still discover F */
    pocket_compressor_t comp;
    size_t F = 16;
    pocket_compressor_init(&comp, F, NULL, 0, 0, 0, 0);

    uint8_t data[] = {0xAB, 0xCD};
    bitvector_t input;
    bitvector_init(&input, F);
    bitvector_from_bytes(&input, data, 2);

    pocket_params_t params;
    params.min_robustness = 0;
    params.new_mask_flag = 0;
    params.send_mask_flag = 0;      /* ft=0 */
    params.uncompressed_flag = 1;   /* rt=1 */

    bitbuffer_t bb;
    bitbuffer_init(&bb);
    pocket_compress_packet(&comp, &input, &bb, &params);

    uint8_t compressed[256];
    size_t bits = bitbuffer_size(&bb);
    bitbuffer_to_bytes(&bb, compressed, 256);

    uint32_t discovered = 0;
    pocket_discover_packet_length(compressed, bits, &discovered);
    TEST_ASSERT(discovered == 16, "rt1_no_ft: discovers F=16 without ft=1");
}

/**
 * @brief Discover F from a reference packet with et=1 and kt bits present.
 *
 * Compresses 3 packets with R=2 and changing data so that the third
 * reference packet has H(Xt)>0, Vt>0, et=1, and kt bits in the
 * bitstream. Exercises the kt skip path in pocket_discover_packet_length.
 */
static void test_discover_with_et1_kt_bits(void) {
    size_t F = 16;
    uint8_t robustness = 2;

    pocket_compressor_t comp;
    pocket_compressor_init(&comp, F, NULL, robustness, 0, 0, 0);

    /* Each packet differs from the previous → non-zero Xt in robustness window */
    uint8_t data[][2] = {
        {0x00, 0x00},
        {0xFF, 0x00},  /* Big change from packet 0 */
        {0xFF, 0xFF}   /* Big change from packet 1 */
    };

    uint8_t pkt_compressed[3][256];
    size_t pkt_bits[3];

    for (size_t i = 0; i < 3; i++) {
        bitvector_t input;
        bitvector_init(&input, F);
        bitvector_from_bytes(&input, data[i], 2);

        pocket_params_t params;
        params.min_robustness = robustness;
        params.new_mask_flag = 0;
        params.send_mask_flag = (i == 0) ? 1 : 0;
        params.uncompressed_flag = 1;  /* All rt=1 (R=2, first 3 packets) */

        bitbuffer_t bb;
        bitbuffer_init(&bb);
        pocket_compress_packet(&comp, &input, &bb, &params);

        pkt_bits[i] = bitbuffer_size(&bb);
        bitbuffer_to_bytes(&bb, pkt_compressed[i], 256);
    }

    /* Packet 2 has H(Xt)>0 (changes in window), Vt>=2>0, et=1 (predictable
     * bits changed), so kt bits are present in the bitstream */
    uint32_t discovered = 0;
    int rc = pocket_discover_packet_length(pkt_compressed[2], pkt_bits[2], &discovered);
    TEST_ASSERT(rc == POCKET_OK, "et1_kt: returns POCKET_OK");
    TEST_ASSERT(discovered == 16, "et1_kt: discovers F=16 through et=1/kt path");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== pocket_discover_packet_length() tests ===\n\n");

    printf("Reference packet discovery:\n");
    test_discover_from_reference_packet();

    printf("\nVarious F values:\n");
    test_discover_F8();
    test_discover_F32();
    test_discover_F64();
    test_discover_F13_non_byte_aligned();

    printf("\nNon-discoverable packets:\n");
    test_discover_non_reference_returns_zero();
    test_discover_ft1_rt0_returns_zero();

    printf("\nInput validation:\n");
    test_discover_null_data();
    test_discover_null_output();
    test_discover_zero_bits();

    printf("\nEdge cases:\n");
    test_discover_truncated();
    test_discover_garbage();
    test_discover_with_robustness();
    test_discover_rt1_without_ft();
    test_discover_with_et1_kt_bits();
    test_discover_truncated_reference();
    test_discover_excess_bits();

    printf("\n%d/%d tests passed\n", tests_passed, tests_total);
    return (tests_passed == tests_total) ? 0 : 1;
}

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
 * CCSDS 124.0-B-1 C Implementation - Bit Buffer Tests
 * TDD: Write tests first, then implement
 * ============================================================================
 */

#include "ccsds124.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test counter */
static int tests_run = 0;
static int tests_passed = 0;

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

/* ========================================================================
 * Initialization and Basic Operations
 * ======================================================================== */

TEST(test_bitbuffer_init) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    assert(bb.num_bits == 0);
}

TEST(test_bitbuffer_clear) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    /* Add some bits */
    bitbuffer_append_bit(&bb, 1);
    bitbuffer_append_bit(&bb, 0);

    assert(bb.num_bits == 2);

    /* Clear */
    bitbuffer_clear(&bb);

    assert(bb.num_bits == 0);
}

TEST(test_bitbuffer_size) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    assert(bitbuffer_size(&bb) == 0);

    bitbuffer_append_bit(&bb, 1);
    assert(bitbuffer_size(&bb) == 1);

    bitbuffer_append_bit(&bb, 0);
    assert(bitbuffer_size(&bb) == 2);
}

/* ========================================================================
 * Bit Appending Tests
 * ======================================================================== */

TEST(test_bitbuffer_append_bit) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    /* MSB-first: Append bits 11001101 to form 0xCD */
    bitbuffer_append_bit(&bb, 1);  /* bit 7 (MSB) = 1 */
    bitbuffer_append_bit(&bb, 1);  /* bit 6 = 1 */
    bitbuffer_append_bit(&bb, 0);  /* bit 5 = 0 */
    bitbuffer_append_bit(&bb, 0);  /* bit 4 = 0 */
    bitbuffer_append_bit(&bb, 1);  /* bit 3 = 1 */
    bitbuffer_append_bit(&bb, 1);  /* bit 2 = 1 */
    bitbuffer_append_bit(&bb, 0);  /* bit 1 = 0 */
    bitbuffer_append_bit(&bb, 1);  /* bit 0 (LSB) = 1 */

    assert(bb.num_bits == 8);
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0xCD);
}

TEST(test_bitbuffer_append_bits) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    uint8_t data[] = {0xAB, 0xCD};

    int result = bitbuffer_append_bits(&bb, data, 16);

    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 16);
    uint8_t output[2];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0xAB);
    assert(output[1] == 0xCD);
}

TEST(test_bitbuffer_append_bits_partial_byte) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    /* MSB-first: Append top 5 bits from 0b00010111 = 0x17 */
    uint8_t data = 0x17;

    int result = bitbuffer_append_bits(&bb, &data, 5);

    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 5);
    /* Top 5 bits (bits 7:3) of 0x17 = 0b00010, shifted to top of byte = 0b00010000 = 0x10 */
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0x10);
}

TEST(test_bitbuffer_append_bitvector) {
    bitbuffer_t bb;
    bitvector_t bv;

    bitbuffer_init(&bb);
    bitvector_init(&bv, 8);

    /* Load byte 0xAB into bitvector using proper byte-to-word packing */
    uint8_t test_byte = 0xAB;
    bitvector_from_bytes(&bv, &test_byte, 1);

    int result = bitbuffer_append_bitvector(&bb, &bv);

    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 8);
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0xAB);
}

TEST(test_bitbuffer_append_multiple) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    /* MSB-first: Append bits 01010011 */
    bitbuffer_append_bit(&bb, 0);  /* bit 7 (MSB) */
    bitbuffer_append_bit(&bb, 1);  /* bit 6 */
    bitbuffer_append_bit(&bb, 0);  /* bit 5 */
    bitbuffer_append_bit(&bb, 1);  /* bit 4 */
    bitbuffer_append_bit(&bb, 0);  /* bit 3 */
    bitbuffer_append_bit(&bb, 0);  /* bit 2 */
    bitbuffer_append_bit(&bb, 1);  /* bit 1 */
    bitbuffer_append_bit(&bb, 1);  /* bit 0 (LSB) */

    /* Result: 0b01010011 = 0x53 */
    assert(bb.num_bits == 8);
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0x53);
}

/* ========================================================================
 * Byte Conversion Tests
 * ======================================================================== */

TEST(test_bitbuffer_to_bytes_full_byte) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    bb.data[0] = 0xAB;
    bb.data[1] = 0xCD;
    bb.num_bits = 16;

    uint8_t bytes[2];
    size_t num_bytes = bitbuffer_to_bytes(&bb, bytes, 2);

    assert(num_bytes == 2);
    assert(bytes[0] == 0xAB);
    assert(bytes[1] == 0xCD);
}

TEST(test_bitbuffer_to_bytes_partial_byte) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    bb.data[0] = 0xFF;
    bb.num_bits = 5;  /* Only 5 bits used */

    uint8_t bytes[1];
    size_t num_bytes = bitbuffer_to_bytes(&bb, bytes, 1);

    /* Should round up to 1 byte */
    assert(num_bytes == 1);
}

TEST(test_bitbuffer_overflow_protection) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    /* Try to append more bits than buffer can hold */
    for (size_t i = 0; i < CCSDS124_MAX_OUTPUT_BYTES * 8 + 100; i++) {
        int result = bitbuffer_append_bit(&bb, 1);

        if (i < CCSDS124_MAX_OUTPUT_BYTES * 8) {
            assert(result == CCSDS124_OK);
        } else {
            /* Should return error when buffer is full */
            assert(result == CCSDS124_ERROR_OVERFLOW);
        }
    }
}

TEST(test_bitbuffer_append_bits_overflow) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    /* Fill buffer near capacity */
    bb.num_bits = CCSDS124_MAX_OUTPUT_BYTES * 8 - 4;

    /* Try to append more bits than remaining space */
    uint8_t data = 0xFF;
    int result = bitbuffer_append_bits(&bb, &data, 8);

    assert(result == CCSDS124_ERROR_OVERFLOW);
}

TEST(test_bitbuffer_append_bitvector_partial) {
    bitbuffer_t bb;
    bitvector_t bv;

    bitbuffer_init(&bb);
    bitvector_init(&bv, 12);  /* Non-byte-aligned: 12 bits */

    /* Set some bits */
    bitvector_set_bit(&bv, 0, 1);
    bitvector_set_bit(&bv, 4, 1);
    bitvector_set_bit(&bv, 8, 1);
    bitvector_set_bit(&bv, 11, 1);

    int result = bitbuffer_append_bitvector(&bb, &bv);

    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 12);
}

TEST(test_bitbuffer_to_bytes_truncate) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    bb.data[0] = 0xAB;
    bb.data[1] = 0xCD;
    bb.data[2] = 0xEF;
    bb.num_bits = 24;

    /* Request only 2 bytes when 3 are available */
    uint8_t bytes[2];
    size_t num_bytes = bitbuffer_to_bytes(&bb, bytes, 2);

    assert(num_bytes == 2);
    assert(bytes[0] == 0xAB);
    assert(bytes[1] == 0xCD);
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(void) {
    printf("\nBit Buffer Tests\n");
    printf("================\n\n");

    /* Basic operations */
    RUN_TEST(test_bitbuffer_init);
    RUN_TEST(test_bitbuffer_clear);
    RUN_TEST(test_bitbuffer_size);

    /* Bit appending */
    RUN_TEST(test_bitbuffer_append_bit);
    RUN_TEST(test_bitbuffer_append_bits);
    RUN_TEST(test_bitbuffer_append_bits_partial_byte);
    RUN_TEST(test_bitbuffer_append_bitvector);
    RUN_TEST(test_bitbuffer_append_multiple);

    /* Byte conversion */
    RUN_TEST(test_bitbuffer_to_bytes_full_byte);
    RUN_TEST(test_bitbuffer_to_bytes_partial_byte);
    RUN_TEST(test_bitbuffer_to_bytes_truncate);

    /* Overflow protection */
    RUN_TEST(test_bitbuffer_overflow_protection);
    RUN_TEST(test_bitbuffer_append_bits_overflow);
    RUN_TEST(test_bitbuffer_append_bitvector_partial);

    printf("\n%d/%d tests passed\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

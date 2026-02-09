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
 * POCKET+ C Implementation - Bit Vector Tests
 * TDD: Write tests first, then implement
 * ============================================================================
 */

#include "pocketplus.h"
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
 * Basic Initialization and Access Tests
 * ======================================================================== */

TEST(test_bitvector_init_valid) {
    bitvector_t bv;
    int result = bitvector_init(&bv, 8);

    assert(result == POCKET_OK);
    assert(bv.length == 8);
    assert(bv.num_words == 1);  /* Ceiling(Ceiling(8/8)/4) = Ceiling(1/4) = 1 */
}

TEST(test_bitvector_init_non_byte_aligned) {
    bitvector_t bv;
    int result = bitvector_init(&bv, 13);

    assert(result == POCKET_OK);
    assert(bv.length == 13);
    assert(bv.num_words == 1);  /* Ceiling(Ceiling(13/8)/4) = Ceiling(2/4) = 1 */
}

TEST(test_bitvector_init_max_size) {
    bitvector_t bv;
    int result = bitvector_init(&bv, POCKET_MAX_PACKET_LENGTH);

    assert(result == POCKET_OK);
    assert(bv.length == POCKET_MAX_PACKET_LENGTH);
}

TEST(test_bitvector_init_too_large) {
    bitvector_t bv;
    int result = bitvector_init(&bv, POCKET_MAX_PACKET_LENGTH + 1);

    assert(result == POCKET_ERROR_INVALID_ARG);
}

TEST(test_bitvector_init_zero) {
    bitvector_t bv;
    int result = bitvector_init(&bv, 0);

    assert(result == POCKET_ERROR_INVALID_ARG);
}

TEST(test_bitvector_zero) {
    bitvector_t bv;
    bitvector_init(&bv, 8);

    /* Set all bits to 1 (big-endian: byte 0 at bits 24-31) */
    bv.data[0] = 0xFF000000;

    /* Zero the vector */
    bitvector_zero(&bv);

    assert(bv.data[0] == 0x00);
}

TEST(test_bitvector_get_set_bit) {
    bitvector_t bv;
    bitvector_init(&bv, 8);
    bitvector_zero(&bv);

    /* Set bit 0 (MSB of first byte in MSB-first indexing) */
    bitvector_set_bit(&bv, 0, 1);
    assert(bitvector_get_bit(&bv, 0) == 1);
    /* With big-endian word packing, byte 0 is at bits 24-31 of word 0, bit 0 is bit 31 */
    assert(bv.data[0] == 0x80000000);

    /* Set bit 7 (LSB of first byte) */
    bitvector_set_bit(&bv, 7, 1);
    assert(bitvector_get_bit(&bv, 7) == 1);
    assert(bv.data[0] == 0x81000000);

    /* Clear bit 0 */
    bitvector_set_bit(&bv, 0, 0);
    assert(bitvector_get_bit(&bv, 0) == 0);
    assert(bv.data[0] == 0x01000000);
}

TEST(test_bitvector_copy) {
    bitvector_t src, dest;
    bitvector_init(&src, 8);
    bitvector_init(&dest, 8);

    src.data[0] = 0xAB;
    bitvector_copy(&dest, &src);

    assert(dest.data[0] == 0xAB);
    assert(dest.length == src.length);
}

/* ========================================================================
 * Bitwise Operation Tests
 * ======================================================================== */

TEST(test_bitvector_xor) {
    bitvector_t a, b, result;
    bitvector_init(&a, 8);
    bitvector_init(&b, 8);
    bitvector_init(&result, 8);

    a.data[0] = 0xB3000000;  /* 10110011 */
    b.data[0] = 0xCA000000;  /* 11001010 */

    bitvector_xor(&result, &a, &b);

    assert(result.data[0] == 0x79000000);  /* 01111001 */
}

TEST(test_bitvector_or) {
    bitvector_t a, b, result;
    bitvector_init(&a, 8);
    bitvector_init(&b, 8);
    bitvector_init(&result, 8);

    a.data[0] = 0xB3000000;  /* 10110011 */
    b.data[0] = 0xCA000000;  /* 11001010 */

    bitvector_or(&result, &a, &b);

    assert(result.data[0] == 0xFB000000);  /* 11111011 */
}

TEST(test_bitvector_and) {
    bitvector_t a, b, result;
    bitvector_init(&a, 8);
    bitvector_init(&b, 8);
    bitvector_init(&result, 8);

    a.data[0] = 0xB3000000;  /* 10110011 */
    b.data[0] = 0xCA000000;  /* 11001010 */

    bitvector_and(&result, &a, &b);

    assert(result.data[0] == 0x82000000);  /* 10000010 */
}

TEST(test_bitvector_not) {
    bitvector_t a, result;
    bitvector_init(&a, 8);
    bitvector_init(&result, 8);

    a.data[0] = 0xB3000000;  /* 10110011 in byte 0 */

    bitvector_not(&result, &a);

    assert(result.data[0] == 0x4C000000);  /* 01001100 in byte 0 */
}

/* ========================================================================
 * Shift and Reverse Tests
 * ======================================================================== */

TEST(test_bitvector_left_shift) {
    bitvector_t a, result;
    bitvector_init(&a, 8);
    bitvector_init(&result, 8);

    a.data[0] = 0xB3000000;  /* 10110011 */

    bitvector_left_shift(&result, &a);

    /* Shift left by 1, insert 0 at LSB: 01100110 */
    assert(result.data[0] == 0x66000000);
}

TEST(test_bitvector_reverse) {
    bitvector_t a, result;
    bitvector_init(&a, 8);
    bitvector_init(&result, 8);

    a.data[0] = 0xB3000000;  /* 10110011 */

    bitvector_reverse(&result, &a);

    /* Reversed: 11001101 */
    assert(result.data[0] == 0xCD000000);
}

/* ========================================================================
 * Utility Function Tests
 * ======================================================================== */

TEST(test_bitvector_hamming_weight) {
    bitvector_t bv;
    bitvector_init(&bv, 8);

    bv.data[0] = 0xB3000000;  /* 10110011 in byte 0 - Five '1' bits */

    size_t weight = bitvector_hamming_weight(&bv);
    assert(weight == 5);
}

TEST(test_bitvector_equals) {
    bitvector_t a, b;
    bitvector_init(&a, 8);
    bitvector_init(&b, 8);

    a.data[0] = 0xAB000000;
    b.data[0] = 0xAB000000;

    assert(bitvector_equals(&a, &b) == 1);

    b.data[0] = 0xCD000000;
    assert(bitvector_equals(&a, &b) == 0);
}

/* ========================================================================
 * Byte Conversion Tests
 * ======================================================================== */

TEST(test_bitvector_from_bytes) {
    bitvector_t bv;
    bitvector_init(&bv, 16);

    uint8_t bytes[] = {0xAB, 0xCD};
    int result = bitvector_from_bytes(&bv, bytes, 2);

    assert(result == POCKET_OK);
    /* Big-endian packing: word[0] = (byte[0] << 24) | (byte[1] << 16) */
    assert(bv.data[0] == 0xABCD0000);
}

TEST(test_bitvector_to_bytes) {
    bitvector_t bv;
    bitvector_init(&bv, 16);

    /* Set word with big-endian packing: bytes {0xAB, 0xCD} */
    bv.data[0] = 0xABCD0000;

    uint8_t bytes[2];
    int result = bitvector_to_bytes(&bv, bytes, 2);

    assert(result == POCKET_OK);
    assert(bytes[0] == 0xAB);
    assert(bytes[1] == 0xCD);
}

/* ========================================================================
 * Edge Case and Error Handling Tests
 * ======================================================================== */

TEST(test_bitvector_copy_different_sizes) {
    bitvector_t src, dest;
    bitvector_init(&src, 8);   /* 1 word */
    bitvector_init(&dest, 64); /* 2 words */

    src.data[0] = 0xAB000000;
    dest.data[0] = 0xFFFFFFFF;
    dest.data[1] = 0xFFFFFFFF;

    bitvector_copy(&dest, &src);

    /* dest should have src's data, and length should match src */
    assert(dest.data[0] == 0xAB000000);
    assert(dest.length == 8);
}

TEST(test_bitvector_xor_different_sizes) {
    bitvector_t a, b, result;
    bitvector_init(&a, 8);     /* 1 word */
    bitvector_init(&b, 64);    /* 2 words */
    bitvector_init(&result, 64);

    a.data[0] = 0xFF000000;
    b.data[0] = 0xAA000000;
    b.data[1] = 0x55555555;

    bitvector_xor(&result, &a, &b);

    /* XOR should use smaller num_words */
    assert(result.data[0] == 0x55000000);
}

TEST(test_bitvector_or_different_sizes) {
    bitvector_t a, b, result;
    bitvector_init(&a, 8);
    bitvector_init(&b, 64);
    bitvector_init(&result, 64);

    a.data[0] = 0x0F000000;
    b.data[0] = 0xF0000000;

    bitvector_or(&result, &a, &b);

    assert(result.data[0] == 0xFF000000);
}

TEST(test_bitvector_hamming_weight_non_aligned) {
    bitvector_t bv;
    /* 40 bits = 1 word + 8 bits - tests extra bits handling */
    bitvector_init(&bv, 40);

    /* Set all bits to 1 */
    bv.data[0] = 0xFFFFFFFF;  /* 32 ones */
    bv.data[1] = 0xFF000000;  /* 8 ones in top byte */

    size_t weight = bitvector_hamming_weight(&bv);
    assert(weight == 40);
}

TEST(test_bitvector_hamming_weight_36_bits) {
    bitvector_t bv;
    /* 36 bits - tests partial extra bits */
    bitvector_init(&bv, 36);

    bv.data[0] = 0xFFFFFFFF;  /* 32 ones */
    bv.data[1] = 0xF0000000;  /* 4 ones in top nibble */

    size_t weight = bitvector_hamming_weight(&bv);
    assert(weight == 36);
}

TEST(test_bitvector_from_bytes_overflow) {
    bitvector_t bv;
    bitvector_init(&bv, 8);  /* Only 1 byte capacity */

    uint8_t bytes[] = {0xAB, 0xCD, 0xEF};  /* 3 bytes */
    int result = bitvector_from_bytes(&bv, bytes, 3);

    assert(result == POCKET_ERROR_OVERFLOW);
}

TEST(test_bitvector_to_bytes_underflow) {
    bitvector_t bv;
    bitvector_init(&bv, 24);  /* 3 bytes */

    bv.data[0] = 0xABCDEF00;

    uint8_t bytes[2];  /* Only 2 bytes buffer */
    int result = bitvector_to_bytes(&bv, bytes, 2);

    assert(result == POCKET_ERROR_UNDERFLOW);
}

TEST(test_bitvector_to_bytes_full_word) {
    bitvector_t bv;
    bitvector_init(&bv, 32);  /* Full word */

    bv.data[0] = 0xAABBCCDD;

    uint8_t bytes[4];
    int result = bitvector_to_bytes(&bv, bytes, 4);

    assert(result == POCKET_OK);
    assert(bytes[0] == 0xAA);
    assert(bytes[1] == 0xBB);
    assert(bytes[2] == 0xCC);
    assert(bytes[3] == 0xDD);
}

TEST(test_bitvector_copy_b_smaller) {
    bitvector_t src, dest;
    bitvector_init(&src, 64);  /* 2 words */
    bitvector_init(&dest, 8);  /* 1 word - smaller */

    src.data[0] = 0xAB000000;
    src.data[1] = 0xCD000000;

    bitvector_copy(&dest, &src);

    assert(dest.data[0] == 0xAB000000);
}

TEST(test_bitvector_xor_b_smaller) {
    bitvector_t a, b, result;
    bitvector_init(&a, 64);    /* 2 words */
    bitvector_init(&b, 8);     /* 1 word - b is smaller */
    bitvector_init(&result, 64);

    a.data[0] = 0xFF000000;
    a.data[1] = 0x12345678;
    b.data[0] = 0xAA000000;

    bitvector_xor(&result, &a, &b);

    assert(result.data[0] == 0x55000000);
}

TEST(test_bitvector_or_b_smaller) {
    bitvector_t a, b, result;
    bitvector_init(&a, 64);    /* 2 words */
    bitvector_init(&b, 8);     /* 1 word - b is smaller */
    bitvector_init(&result, 64);

    a.data[0] = 0x0F000000;
    b.data[0] = 0xF0000000;

    bitvector_or(&result, &a, &b);

    assert(result.data[0] == 0xFF000000);
}

TEST(test_bitvector_not_16bits) {
    bitvector_t a, result;
    bitvector_init(&a, 16);      /* 2 bytes - triggers full byte mask path */
    bitvector_init(&result, 16);

    a.data[0] = 0xABCD0000;

    bitvector_not(&result, &a);

    /* NOT of 0xABCD = 0x5432 (for 16 bits) */
    assert(result.data[0] == 0x54320000);
}

TEST(test_bitvector_not_non_byte_aligned) {
    /* Regression test: NOT with non-byte-aligned length must mask
     * the HIGH bits (MSB-aligned packing), not the low bits. */
    bitvector_t a, result;
    bitvector_init(&a, 4);       /* 4 bits in 1 byte */
    bitvector_init(&result, 4);

    a.data[0] = 0x00000000;     /* 0000 in top 4 bits */

    bitvector_not(&result, &a);

    /* NOT 0000 = 1111 in top 4 bits of byte = 0xF0000000, NOT 0x0F000000 */
    assert(result.data[0] == 0xF0000000);
}

TEST(test_bitvector_reverse_16bits) {
    bitvector_t a, result;
    bitvector_init(&a, 16);      /* 2 bytes in 1 word */
    bitvector_init(&result, 16);

    /* Set pattern 0xABCD */
    a.data[0] = 0xABCD0000;

    bitvector_reverse(&result, &a);

    /* Reversed bits of 0xABCD (16 bits) */
    /* 0xABCD = 1010101111001101 reversed = 1011001111010101 = 0xB3D5 */
    assert(result.data[0] == 0xB3D50000);
}

TEST(test_bitvector_and_b_smaller) {
    bitvector_t a, b, result;
    bitvector_init(&a, 64);    /* 2 words */
    bitvector_init(&b, 8);     /* 1 word - b is smaller */
    bitvector_init(&result, 64);

    a.data[0] = 0xFF000000;
    b.data[0] = 0xAA000000;

    bitvector_and(&result, &a, &b);

    assert(result.data[0] == 0xAA000000);
}

TEST(test_bitvector_hamming_weight_excess_bits) {
    bitvector_t bv;
    /* 36 bits - last word has excess capacity with some garbage bits */
    bitvector_init(&bv, 36);

    bv.data[0] = 0xFFFFFFFF;  /* 32 ones */
    /* Top 4 bits meaningful (36-32=4), set garbage in bottom 4 bits */
    bv.data[1] = 0xF000000F;  /* 4 meaningful ones at top + 4 garbage ones at bottom */

    size_t weight = bitvector_hamming_weight(&bv);
    /* popcount(word0) + popcount(word1) = 32 + 8 = 40
     * extra_bits = 36 % 32 = 4
     * mask = 0xF (bottom 4 bits)
     * extra_word = 0xF000000F & 0xF = 0xF (4 ones)
     * Final = 40 - 4 = 36 (correct: ignores garbage bits) */
    assert(weight == 36);
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(void) {
    printf("\nBit Vector Tests\n");
    printf("================\n\n");

    /* Basic initialization */
    RUN_TEST(test_bitvector_init_valid);
    RUN_TEST(test_bitvector_init_non_byte_aligned);
    RUN_TEST(test_bitvector_init_max_size);
    RUN_TEST(test_bitvector_init_too_large);
    RUN_TEST(test_bitvector_init_zero);
    RUN_TEST(test_bitvector_zero);
    RUN_TEST(test_bitvector_get_set_bit);
    RUN_TEST(test_bitvector_copy);

    /* Bitwise operations */
    RUN_TEST(test_bitvector_xor);
    RUN_TEST(test_bitvector_or);
    RUN_TEST(test_bitvector_and);
    RUN_TEST(test_bitvector_not);

    /* Shift and reverse */
    RUN_TEST(test_bitvector_left_shift);
    RUN_TEST(test_bitvector_reverse);

    /* Utilities */
    RUN_TEST(test_bitvector_hamming_weight);
    RUN_TEST(test_bitvector_equals);

    /* Byte conversion */
    RUN_TEST(test_bitvector_from_bytes);
    RUN_TEST(test_bitvector_to_bytes);

    /* Edge cases and error handling */
    RUN_TEST(test_bitvector_copy_different_sizes);
    RUN_TEST(test_bitvector_xor_different_sizes);
    RUN_TEST(test_bitvector_or_different_sizes);
    RUN_TEST(test_bitvector_hamming_weight_non_aligned);
    RUN_TEST(test_bitvector_hamming_weight_36_bits);
    RUN_TEST(test_bitvector_from_bytes_overflow);
    RUN_TEST(test_bitvector_to_bytes_underflow);
    RUN_TEST(test_bitvector_to_bytes_full_word);
    RUN_TEST(test_bitvector_copy_b_smaller);
    RUN_TEST(test_bitvector_xor_b_smaller);
    RUN_TEST(test_bitvector_or_b_smaller);
    RUN_TEST(test_bitvector_and_b_smaller);
    RUN_TEST(test_bitvector_not_16bits);
    RUN_TEST(test_bitvector_not_non_byte_aligned);
    RUN_TEST(test_bitvector_reverse_16bits);
    RUN_TEST(test_bitvector_hamming_weight_excess_bits);

    printf("\n%d/%d tests passed\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

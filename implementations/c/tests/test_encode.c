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
 * CCSDS 124.0-B-1 C Implementation - Encoding Functions Tests
 * TDD: Write tests first, then implement
 *
 * Tests for CCSDS 124.0-B-1 Section 5.2:
 * - COUNT encoding (Section 5.2.2, Table 5-1, Equation 9)
 * - RLE encoding (Section 5.2.3, Equation 10)
 * - Bit Extraction (Section 5.2.4, Equation 11)
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

/* Forward declarations for encoding functions */
int ccsds124_count_encode(bitbuffer_t *output, uint32_t value);
int ccsds124_rle_encode(bitbuffer_t *output, const bitvector_t *input);
int ccsds124_bit_extract(bitbuffer_t *output, const bitvector_t *data, const bitvector_t *mask);

/* ========================================================================
 * COUNT Encoding Tests (CCSDS Section 5.2.2, Table 5-1)
 *
 * Encoding for positive integers 1 ≤ A ≤ 2^16 - 1:
 *   A = 1        → '0'
 *   2 ≤ A ≤ 33   → '110' ∥ BIT₅(A-2)
 *   A ≥ 34       → '111' ∥ BIT_E(A-2) where E = 2⌊log₂(A-2)+1⌋ - 6
 * ======================================================================== */

TEST(test_count_encode_1) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    int result = ccsds124_count_encode(&bb, 1);

    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 1);
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0x00);  /* '0' */
}

TEST(test_count_encode_2) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    int result = ccsds124_count_encode(&bb, 2);

    /* 2 → '110' ∥ BIT₅(0) = '110' ∥ '00000' = '11000000' = 0xC0 */
    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 8);
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0xC0);
}

TEST(test_count_encode_3) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    int result = ccsds124_count_encode(&bb, 3);

    /* 3 → '110' ∥ BIT₅(1) = '110' ∥ '00001' = '11000001' = 0xC1 */
    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 8);
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0xC1);
}

TEST(test_count_encode_33) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    int result = ccsds124_count_encode(&bb, 33);

    /* 33 → '110' ∥ BIT₅(31) = '110' ∥ '11111' = '11011111' = 0xDF */
    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 8);
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0xDF);
}

TEST(test_count_encode_34) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    int result = ccsds124_count_encode(&bb, 34);

    /* 34 → '111' ∥ BIT_E(32) where E = 2⌊log₂(32)+1⌋ - 6 = 2*6 - 6 = 6 */
    /* '111' ∥ BIT₆(32) = '111' ∥ '100000' */
    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 9);
}

TEST(test_count_encode_4) {
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    int result = ccsds124_count_encode(&bb, 4);

    /* 4 → '110' ∥ BIT₅(2) = '110' ∥ '00010' = '11000010' = 0xC2 */
    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 8);
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0xC2);
}

TEST(test_count_encode_example_from_spec) {
    /* Example from ALGORITHM.md: COUNT(4) */
    bitbuffer_t bb;
    bitbuffer_init(&bb);

    int result = ccsds124_count_encode(&bb, 4);

    assert(result == CCSDS124_OK);
    /* 4 is in range 2-33, so '110' ∥ BIT₅(2) */
    assert(bb.num_bits == 8);
}

/* ========================================================================
 * RLE Encoding Tests (CCSDS Section 5.2.3, Equation 10)
 *
 * RLE(a) = COUNT(C₀) ∥ ... ∥ COUNT(C_{H(a)-1}) ∥ '10'
 * where Cᵢ = 1 + (count of consecutive 0s before i-th '1' bit)
 * ======================================================================== */

TEST(test_rle_encode_simple) {
    bitbuffer_t bb;
    bitvector_t bv;

    bitbuffer_init(&bb);
    bitvector_init(&bv, 8);

    /* Simple pattern: 00010001 (two '1' bits at positions 3 and 7) */
    bitvector_set_bit(&bv, 3, 1);
    bitvector_set_bit(&bv, 7, 1);

    int result = ccsds124_rle_encode(&bb, &bv);

    assert(result == CCSDS124_OK);

    /* C₀ = 4 (3 zeros + 1 before first '1' at position 3)
       C₁ = 5 (4 zeros + 1 before second '1' at position 7)
       RLE = COUNT(4) ∥ COUNT(5) ∥ '10' */

    /* Should have some output */
    assert(bb.num_bits > 0);
}

TEST(test_rle_encode_all_zeros) {
    bitbuffer_t bb;
    bitvector_t bv;

    bitbuffer_init(&bb);
    bitvector_init(&bv, 8);
    bitvector_zero(&bv);

    int result = ccsds124_rle_encode(&bb, &bv);

    assert(result == CCSDS124_OK);

    /* No '1' bits, so just '10' terminator MSB-first */
    assert(bb.num_bits == 2);
    /* '10' → bits 1,0 → 0b10000000 = 0x80 */
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0x80);
}

TEST(test_rle_encode_example_from_algorithm) {
    /* Example from ALGORITHM.md:
       a = '0001000001000010000010000100000010000000000000000'
       Has '1' bits with counts: C₀=4, C₁=6, C₂=1, C₃=6, C₄=5, C₅=7, C₆=3 */

    bitbuffer_t bb;
    bitvector_t bv;

    bitbuffer_init(&bb);
    bitvector_init(&bv, 47);

    /* Set the '1' bits at correct positions */
    bitvector_set_bit(&bv, 3, 1);   /* After 3 zeros */
    bitvector_set_bit(&bv, 9, 1);   /* After 5 more zeros (total from 4-9) */
    bitvector_set_bit(&bv, 14, 1);  /* After 4 more zeros */
    bitvector_set_bit(&bv, 20, 1);  /* After 5 more zeros */
    bitvector_set_bit(&bv, 25, 1);  /* After 4 more zeros */
    bitvector_set_bit(&bv, 31, 1);  /* After 5 more zeros */
    bitvector_set_bit(&bv, 44, 1);  /* After 12 more zeros */

    int result = ccsds124_rle_encode(&bb, &bv);

    assert(result == CCSDS124_OK);
    /* Should produce COUNT(4) ∥ COUNT(6) ∥ ... ∥ COUNT(13) ∥ '10' */
    assert(bb.num_bits > 0);
}

/* ========================================================================
 * Bit Extraction Tests (CCSDS Section 5.2.4, Equation 11)
 *
 * BE(a, b) = extracts bits from 'a' at positions where 'b' has '1' bits
 * Example: BE('10110011', '01001010') = '001'
 * ======================================================================== */

TEST(test_bit_extract_simple) {
    bitbuffer_t bb;
    bitvector_t data, mask;

    bitbuffer_init(&bb);
    bitvector_init(&data, 8);
    bitvector_init(&mask, 8);

    /* data = 10110011 = 0xB3 */
    data.data[0] = 0xB3000000;

    /* mask = 01001010 = 0x4A (extract bits at positions 1, 4, 6 with MSB-first) */
    mask.data[0] = 0x4A000000;

    int result = ccsds124_bit_extract(&bb, &data, &mask);

    assert(result == CCSDS124_OK);

    /* MSB-first indexing:
       Mask has '1' at positions 1, 4, 6
       Data at those positions: data[1]=0, data[4]=0, data[6]=1
       CCSDS BE extracts highest to lowest: data[6], data[4], data[1] = 1,0,0
       Result: 100 (binary) → 0b10000000 = 0x80 */

    assert(bb.num_bits == 3);
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0x80);
}

TEST(test_bit_extract_no_mask) {
    bitbuffer_t bb;
    bitvector_t data, mask;

    bitbuffer_init(&bb);
    bitvector_init(&data, 8);
    bitvector_init(&mask, 8);

    data.data[0] = 0xFF000000;
    bitvector_zero(&mask);  /* No bits set in mask */

    int result = ccsds124_bit_extract(&bb, &data, &mask);

    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 0);  /* No bits extracted */
}

TEST(test_bit_extract_all_mask) {
    bitbuffer_t bb;
    bitvector_t data, mask;

    bitbuffer_init(&bb);
    bitvector_init(&data, 8);
    bitvector_init(&mask, 8);

    data.data[0] = 0xAB000000;  /* 10101011 */
    mask.data[0] = 0xFF000000;  /* All bits set */

    int result = ccsds124_bit_extract(&bb, &data, &mask);

    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 8);

    /* CCSDS BE extracts from highest to lowest position
       Input: 10101011 (bits 0-7)
       Extract order: bit[7,6,5,4,3,2,1,0] = 1,1,0,1,0,1,0,1
       Output: 11010101 = 0xD5 (bit-reversed) */
    uint8_t output[1];
    bitbuffer_to_bytes(&bb, output, sizeof(output));
    assert(output[0] == 0xD5);
}

TEST(test_bit_extract_length_mismatch) {
    bitbuffer_t bb;
    bitvector_t data, mask;

    bitbuffer_init(&bb);
    bitvector_init(&data, 8);
    bitvector_init(&mask, 16);  /* Different length */

    int result = ccsds124_bit_extract(&bb, &data, &mask);

    assert(result == CCSDS124_ERROR_INVALID_ARG);
}

TEST(test_bit_extract_forward_length_mismatch) {
    bitbuffer_t bb;
    bitvector_t data, mask;

    bitbuffer_init(&bb);
    bitvector_init(&data, 8);
    bitvector_init(&mask, 16);  /* Different length */

    int result = ccsds124_bit_extract_forward(&bb, &data, &mask);

    assert(result == CCSDS124_ERROR_INVALID_ARG);
}

TEST(test_bit_extract_forward_no_mask) {
    bitbuffer_t bb;
    bitvector_t data, mask;

    bitbuffer_init(&bb);
    bitvector_init(&data, 8);
    bitvector_init(&mask, 8);

    data.data[0] = 0xFF000000;
    bitvector_zero(&mask);  /* No bits set in mask */

    int result = ccsds124_bit_extract_forward(&bb, &data, &mask);

    assert(result == CCSDS124_OK);
    assert(bb.num_bits == 0);  /* No bits extracted */
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(void) {
    printf("\nEncoding Functions Tests\n");
    printf("========================\n\n");

    printf("COUNT Encoding Tests:\n");
    RUN_TEST(test_count_encode_1);
    RUN_TEST(test_count_encode_2);
    RUN_TEST(test_count_encode_3);
    RUN_TEST(test_count_encode_4);
    RUN_TEST(test_count_encode_33);
    RUN_TEST(test_count_encode_34);
    RUN_TEST(test_count_encode_example_from_spec);

    printf("\nRLE Encoding Tests:\n");
    RUN_TEST(test_rle_encode_simple);
    RUN_TEST(test_rle_encode_all_zeros);
    RUN_TEST(test_rle_encode_example_from_algorithm);

    printf("\nBit Extraction Tests:\n");
    RUN_TEST(test_bit_extract_simple);
    RUN_TEST(test_bit_extract_no_mask);
    RUN_TEST(test_bit_extract_all_mask);
    RUN_TEST(test_bit_extract_length_mismatch);
    RUN_TEST(test_bit_extract_forward_length_mismatch);
    RUN_TEST(test_bit_extract_forward_no_mask);

    printf("\n%d/%d tests passed\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

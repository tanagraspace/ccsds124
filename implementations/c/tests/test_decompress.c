/**
 * @file test_decompress.c
 * @brief Unit tests for CCSDS 124.0-B-1 decompression functions.
 *
 * Tests follow TDD methodology - tests are written before implementation.
 * Code follows MISRA C:2012 guidelines.
 */

#include "ccsds124.h"
#include <stdio.h>
#include <string.h>

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
 * COUNT Decoding Tests (inverse of COUNT encoding)
 *
 * CCSDS COUNT encoding:
 *   A = 1      → '0'
 *   2 ≤ A ≤ 33 → '110' ∥ BIT₅(A-2)
 *   A ≥ 34    → '111' ∥ BIT_E(A-2)
 *   Terminator → '10'
 * ============================================================================ */

/**
 * @brief Test COUNT decode of value 1 (encoded as '0').
 */
static void test_count_decode_1(void) {
    /* '0' decodes to 1 */
    uint8_t data[1] = {0x00};  /* 0b00000000 - first bit is 0 */
    bitreader_t reader;
    bitreader_init(&reader, data, 1U);

    uint32_t value = 0U;
    int result = ccsds124_count_decode(&reader, &value);

    TEST_ASSERT(result == CCSDS124_OK, "test_count_decode_1: returns OK");
    TEST_ASSERT(value == 1U, "test_count_decode_1: value is 1");
    TEST_ASSERT(bitreader_position(&reader) == 1U, "test_count_decode_1: consumed 1 bit");
}

/**
 * @brief Test COUNT decode of terminator (encoded as '10').
 */
static void test_count_decode_terminator(void) {
    /* '10' decodes to 0 (terminator) */
    uint8_t data[1] = {0x80};  /* 0b10000000 */
    bitreader_t reader;
    bitreader_init(&reader, data, 2U);

    uint32_t value = 0U;
    int result = ccsds124_count_decode(&reader, &value);

    TEST_ASSERT(result == CCSDS124_OK, "test_count_decode_terminator: returns OK");
    TEST_ASSERT(value == 0U, "test_count_decode_terminator: value is 0");
    TEST_ASSERT(bitreader_position(&reader) == 2U, "test_count_decode_terminator: consumed 2 bits");
}

/**
 * @brief Test COUNT decode of value 2 (encoded as '110' + 00000).
 */
static void test_count_decode_2(void) {
    /* '110' + BIT₅(0) = '11000000' = 0xC0 */
    uint8_t data[1] = {0xC0};  /* 0b11000000 */
    bitreader_t reader;
    bitreader_init(&reader, data, 8U);

    uint32_t value = 0U;
    int result = ccsds124_count_decode(&reader, &value);

    TEST_ASSERT(result == CCSDS124_OK, "test_count_decode_2: returns OK");
    TEST_ASSERT(value == 2U, "test_count_decode_2: value is 2");
    TEST_ASSERT(bitreader_position(&reader) == 8U, "test_count_decode_2: consumed 8 bits");
}

/**
 * @brief Test COUNT decode of value 33 (encoded as '110' + 11111).
 */
static void test_count_decode_33(void) {
    /* '110' + BIT₅(31) = '11011111' = 0xDF */
    uint8_t data[1] = {0xDF};  /* 0b11011111 */
    bitreader_t reader;
    bitreader_init(&reader, data, 8U);

    uint32_t value = 0U;
    int result = ccsds124_count_decode(&reader, &value);

    TEST_ASSERT(result == CCSDS124_OK, "test_count_decode_33: returns OK");
    TEST_ASSERT(value == 33U, "test_count_decode_33: value is 33");
}

/**
 * @brief Test COUNT decode of value 34 (first value using extended encoding).
 *
 * A=34: '111' + BIT_E(32) where E = 2*floor(log2(32)+1) - 6 = 2*6-6 = 6
 * BIT_6(32) = 100000
 * Full: '111100000' = 0xF0 0x00 (9 bits, but only need first byte + 1 bit)
 */
static void test_count_decode_34(void) {
    /* '111' + '100000' = 0b11110000 0... = 0xF0 */
    uint8_t data[2] = {0xF0, 0x00};
    bitreader_t reader;
    bitreader_init(&reader, data, 16U);

    uint32_t value = 0U;
    int result = ccsds124_count_decode(&reader, &value);

    TEST_ASSERT(result == CCSDS124_OK, "test_count_decode_34: returns OK");
    TEST_ASSERT(value == 34U, "test_count_decode_34: value is 34");
}

/**
 * @brief Test round-trip: encode then decode same value.
 */
static void test_count_roundtrip(void) {
    uint32_t test_values[] = {1U, 2U, 10U, 33U, 34U, 100U, 1000U};
    size_t num_values = sizeof(test_values) / sizeof(test_values[0]);
    int all_passed = 1;

    for (size_t i = 0U; i < num_values; i++) {
        /* Encode */
        bitbuffer_t encoded;
        bitbuffer_init(&encoded);
        (void)ccsds124_count_encode(&encoded, test_values[i]);

        /* Decode */
        uint8_t data[16];
        (void)bitbuffer_to_bytes(&encoded, data, sizeof(data));

        bitreader_t reader;
        bitreader_init(&reader, data, encoded.num_bits);

        uint32_t decoded = 0U;
        (void)ccsds124_count_decode(&reader, &decoded);

        if (decoded != test_values[i]) {
            all_passed = 0;
            printf("    Round-trip failed for %u: got %u\n", test_values[i], decoded);
        }
    }

    TEST_ASSERT(all_passed != 0, "test_count_roundtrip: all values round-trip correctly");
}

/* ============================================================================
 * RLE Decoding Tests (inverse of RLE encoding)
 * ============================================================================ */

/**
 * @brief Test RLE decode of all-zeros vector.
 *
 * All zeros: just terminator '10'
 */
static void test_rle_decode_all_zeros(void) {
    /* RLE of all zeros is just '10' (terminator) */
    uint8_t data[1] = {0x80};  /* 0b10000000 */
    bitreader_t reader;
    bitreader_init(&reader, data, 2U);

    bitvector_t result;
    bitvector_init(&result, 8U);

    int status = ccsds124_rle_decode(&reader, &result, 8U);

    TEST_ASSERT(status == CCSDS124_OK, "test_rle_decode_all_zeros: returns OK");
    TEST_ASSERT(bitvector_hamming_weight(&result) == 0U, "test_rle_decode_all_zeros: all bits zero");
}

/**
 * @brief Test RLE round-trip for various bit patterns.
 */
static void test_rle_roundtrip(void) {
    int all_passed = 1;

    /* Test patterns */
    uint8_t patterns[][2] = {
        {0x00, 0x00},  /* All zeros */
        {0x80, 0x00},  /* Single bit at MSB */
        {0x00, 0x01},  /* Single bit at LSB */
        {0xFF, 0xFF},  /* All ones */
        {0xAA, 0xAA},  /* Alternating */
        {0x55, 0x55},  /* Alternating inverse */
    };

    for (size_t p = 0U; p < 6U; p++) {
        /* Create input vector */
        bitvector_t input;
        bitvector_init(&input, 16U);
        bitvector_from_bytes(&input, patterns[p], 2U);

        /* Encode */
        bitbuffer_t encoded;
        bitbuffer_init(&encoded);
        (void)ccsds124_rle_encode(&encoded, &input);

        /* Get encoded bytes */
        uint8_t enc_data[16];
        (void)bitbuffer_to_bytes(&encoded, enc_data, sizeof(enc_data));

        /* Decode */
        bitreader_t reader;
        bitreader_init(&reader, enc_data, encoded.num_bits);

        bitvector_t decoded;
        bitvector_init(&decoded, 16U);
        (void)ccsds124_rle_decode(&reader, &decoded, 16U);

        /* Compare */
        if (bitvector_equals(&input, &decoded) == 0) {
            all_passed = 0;
            printf("    RLE round-trip failed for pattern %zu\n", p);
        }
    }

    TEST_ASSERT(all_passed != 0, "test_rle_roundtrip: all patterns round-trip correctly");
}

/* ============================================================================
 * Packet Decompression Tests
 * ============================================================================ */

/**
 * @brief Test decompression of a simple uncompressed packet.
 *
 * First packet with rt=1 contains full input data.
 */
static void test_decompress_uncompressed_packet(void) {
    /* Create a simple input packet */
    uint8_t input_data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    size_t packet_bits = 32U;

    /* Compress it */
    ccsds124_compressor_t comp;
    ccsds124_compressor_init(&comp, packet_bits, NULL, 1U, 10, 20, 50);

    bitvector_t input;
    bitvector_init(&input, packet_bits);
    bitvector_from_bytes(&input, input_data, 4U);

    bitbuffer_t compressed;
    bitbuffer_init(&compressed);

    ccsds124_params_t params = {1U, 0, 1, 1};  /* First packet: ft=1, rt=1 */
    (void)ccsds124_compress_packet(&comp, &input, &compressed, &params);

    /* Now decompress */
    uint8_t comp_bytes[64];
    (void)bitbuffer_to_bytes(&compressed, comp_bytes, sizeof(comp_bytes));

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, packet_bits, NULL, 1U);

    bitvector_t output;
    bitvector_init(&output, packet_bits);

    bitreader_t reader;
    bitreader_init(&reader, comp_bytes, compressed.num_bits);

    int result = ccsds124_decompress_packet(&decomp, &reader, &output);

    /* Verify */
    uint8_t output_data[4];
    bitvector_to_bytes(&output, output_data, 4U);

    int data_matches = (memcmp(input_data, output_data, 4U) == 0) ? 1 : 0;

    TEST_ASSERT(result == CCSDS124_OK, "test_decompress_uncompressed_packet: returns OK");
    TEST_ASSERT(data_matches != 0, "test_decompress_uncompressed_packet: data matches");
}

/**
 * @brief Test round-trip compression/decompression of multiple packets.
 */
static void test_decompress_roundtrip_multiple(void) {
    /* Create test data: 10 packets of 8 bytes each */
    uint8_t input_data[80];
    for (size_t i = 0U; i < 80U; i++) {
        input_data[i] = (uint8_t)(i * 3U);  /* Some pattern */
    }

    size_t packet_bits = 64U;

    /* Compress all packets */
    ccsds124_compressor_t comp;
    ccsds124_compressor_init(&comp, packet_bits, NULL, 1U, 10, 20, 50);

    uint8_t compressed_data[1024];
    size_t compressed_size = 0U;

    (void)ccsds124_compress(&comp, input_data, 80U, compressed_data, sizeof(compressed_data), &compressed_size);

    /* Decompress all packets */
    uint8_t output_data[80];
    size_t output_size = 0U;

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, packet_bits, NULL, 1U);

    int result = ccsds124_decompress(&decomp, compressed_data, compressed_size,
                                    output_data, sizeof(output_data), &output_size);

    /* Verify */
    int data_matches = (memcmp(input_data, output_data, 80U) == 0) ? 1 : 0;

    TEST_ASSERT(result == CCSDS124_OK, "test_decompress_roundtrip_multiple: returns OK");
    TEST_ASSERT(output_size == 80U, "test_decompress_roundtrip_multiple: correct size");
    TEST_ASSERT(data_matches != 0, "test_decompress_roundtrip_multiple: data matches");
}

/* ============================================================================
 * Bit Reader Tests
 * ============================================================================ */

/**
 * @brief Test bit reader initialization.
 */
static void test_bitreader_init(void) {
    uint8_t data[4] = {0xAB, 0xCD, 0xEF, 0x12};
    bitreader_t reader;

    bitreader_init(&reader, data, 32U);

    TEST_ASSERT(bitreader_position(&reader) == 0U, "test_bitreader_init: position is 0");
    TEST_ASSERT(bitreader_remaining(&reader) == 32U, "test_bitreader_init: 32 bits remaining");
}

/**
 * @brief Test reading single bits.
 */
static void test_bitreader_read_bit(void) {
    uint8_t data[1] = {0xA5};  /* 0b10100101 */
    bitreader_t reader;
    bitreader_init(&reader, data, 8U);

    /* Read bits MSB first */
    int bit0 = bitreader_read_bit(&reader);  /* 1 */
    int bit1 = bitreader_read_bit(&reader);  /* 0 */
    int bit2 = bitreader_read_bit(&reader);  /* 1 */
    int bit3 = bitreader_read_bit(&reader);  /* 0 */

    TEST_ASSERT(bit0 == 1, "test_bitreader_read_bit: bit 0 is 1");
    TEST_ASSERT(bit1 == 0, "test_bitreader_read_bit: bit 1 is 0");
    TEST_ASSERT(bit2 == 1, "test_bitreader_read_bit: bit 2 is 1");
    TEST_ASSERT(bit3 == 0, "test_bitreader_read_bit: bit 3 is 0");
    TEST_ASSERT(bitreader_position(&reader) == 4U, "test_bitreader_read_bit: position is 4");
}

/**
 * @brief Test reading multiple bits as value.
 */
static void test_bitreader_read_bits(void) {
    uint8_t data[2] = {0xAB, 0xCD};  /* 0b10101011 11001101 */
    bitreader_t reader;
    bitreader_init(&reader, data, 16U);

    uint32_t val4 = bitreader_read_bits(&reader, 4U);   /* 0b1010 = 10 */
    uint32_t val8 = bitreader_read_bits(&reader, 8U);   /* 0b10111100 = 188 */

    TEST_ASSERT(val4 == 10U, "test_bitreader_read_bits: first 4 bits = 10");
    TEST_ASSERT(val8 == 188U, "test_bitreader_read_bits: next 8 bits = 188");
}

/* ============================================================================
 * NULL Argument and Error Path Tests
 * ============================================================================ */

static void test_count_decode_null_args(void) {
    uint8_t data[1] = {0xFF};
    bitreader_t reader;
    uint32_t value;

    bitreader_init(&reader, data, 8U);

    /* NULL reader */
    int result = ccsds124_count_decode(NULL, &value);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_count_decode_null_args: NULL reader fails");

    /* NULL value */
    result = ccsds124_count_decode(&reader, NULL);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_count_decode_null_args: NULL value fails");
}

static void test_count_decode_empty_reader(void) {
    uint8_t data[1] = {0xFF};
    bitreader_t reader;
    uint32_t value;

    /* Initialize with 0 bits */
    bitreader_init(&reader, data, 0U);

    int result = ccsds124_count_decode(&reader, &value);
    TEST_ASSERT(result == CCSDS124_ERROR_UNDERFLOW, "test_count_decode_empty_reader: empty reader fails");
}

static void test_decompressor_init_null(void) {
    int result = ccsds124_decompressor_init(NULL, 8, NULL, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompressor_init_null: NULL decomp fails");
}

static void test_decompress_packet_null_args(void) {
    ccsds124_decompressor_t decomp;
    bitreader_t reader;
    bitvector_t output;
    uint8_t data[1] = {0xFF};

    ccsds124_decompressor_init(&decomp, 8, NULL, 0);
    bitreader_init(&reader, data, 8U);
    bitvector_init(&output, 8);

    /* NULL decomp */
    int result = ccsds124_decompress_packet(NULL, &reader, &output);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompress_packet_null_args: NULL decomp fails");

    /* NULL reader */
    result = ccsds124_decompress_packet(&decomp, NULL, &output);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompress_packet_null_args: NULL reader fails");

    /* NULL output */
    result = ccsds124_decompress_packet(&decomp, &reader, NULL);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompress_packet_null_args: NULL output fails");
}

static void test_decompress_null_args(void) {
    ccsds124_decompressor_t decomp;
    uint8_t input[4] = {0};
    uint8_t output[4];
    size_t output_size;

    ccsds124_decompressor_init(&decomp, 8, NULL, 0);

    /* NULL decomp */
    int result = ccsds124_decompress(NULL, input, 4, output, 4, &output_size);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompress_null_args: NULL decomp fails");

    /* NULL input */
    result = ccsds124_decompress(&decomp, NULL, 4, output, 4, &output_size);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompress_null_args: NULL input fails");

    /* NULL output buffer */
    result = ccsds124_decompress(&decomp, input, 4, NULL, 4, &output_size);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompress_null_args: NULL output fails");

    /* NULL output_size */
    result = ccsds124_decompress(&decomp, input, 4, output, 4, NULL);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompress_null_args: NULL output_size fails");
}

static void test_rle_decode_null_args(void) {
    uint8_t data[1] = {0x80};
    bitreader_t reader;
    bitvector_t result;

    bitreader_init(&reader, data, 8U);
    bitvector_init(&result, 8);

    /* NULL reader */
    int status = ccsds124_rle_decode(NULL, &result, 8U);
    TEST_ASSERT(status == CCSDS124_ERROR_INVALID_ARG, "test_rle_decode_null_args: NULL reader fails");

    /* NULL result */
    status = ccsds124_rle_decode(&reader, NULL, 8U);
    TEST_ASSERT(status == CCSDS124_ERROR_INVALID_ARG, "test_rle_decode_null_args: NULL result fails");
}

static void test_bit_insert_null_args(void) {
    uint8_t data[1] = {0xFF};
    bitreader_t reader;
    bitvector_t bv_data, mask;

    bitreader_init(&reader, data, 8U);
    bitvector_init(&bv_data, 8);
    bitvector_init(&mask, 8);

    /* NULL reader */
    int status = ccsds124_bit_insert(NULL, &bv_data, &mask);
    TEST_ASSERT(status == CCSDS124_ERROR_INVALID_ARG, "test_bit_insert_null_args: NULL reader fails");

    /* NULL data */
    status = ccsds124_bit_insert(&reader, NULL, &mask);
    TEST_ASSERT(status == CCSDS124_ERROR_INVALID_ARG, "test_bit_insert_null_args: NULL data fails");

    /* NULL mask */
    status = ccsds124_bit_insert(&reader, &bv_data, NULL);
    TEST_ASSERT(status == CCSDS124_ERROR_INVALID_ARG, "test_bit_insert_null_args: NULL mask fails");
}

static void test_bit_insert_length_mismatch(void) {
    uint8_t data[2] = {0xFF, 0xFF};
    bitreader_t reader;
    bitvector_t bv_data, mask;

    bitreader_init(&reader, data, 16U);
    bitvector_init(&bv_data, 8);   /* 8 bits */
    bitvector_init(&mask, 16);     /* 16 bits - mismatch */

    int status = ccsds124_bit_insert(&reader, &bv_data, &mask);
    TEST_ASSERT(status == CCSDS124_ERROR_INVALID_ARG, "test_bit_insert_length_mismatch: mismatched lengths fails");
}

static void test_decompress_output_overflow(void) {
    /* Compress some data first */
    uint8_t input_data[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
    size_t packet_bits = 32U;

    ccsds124_compressor_t comp;
    ccsds124_compressor_init(&comp, packet_bits, NULL, 1U, 10, 20, 50);

    uint8_t compressed[256];
    size_t compressed_size = 0U;
    (void)ccsds124_compress(&comp, input_data, 8U, compressed, sizeof(compressed), &compressed_size);

    /* Try to decompress into a buffer that's too small */
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, packet_bits, NULL, 1U);

    uint8_t output[2];  /* Only 2 bytes, need 8 */
    size_t output_size = 0U;

    int result = ccsds124_decompress(&decomp, compressed, compressed_size, output, sizeof(output), &output_size);
    TEST_ASSERT(result == CCSDS124_ERROR_OVERFLOW, "test_decompress_output_overflow: small buffer fails");
}

/* ============================================================================
 * Decompressor Init Error Tests
 * ============================================================================ */

static void test_decompressor_init_invalid_length(void) {
    ccsds124_decompressor_t decomp;

    /* F = 0 should fail */
    int result = ccsds124_decompressor_init(&decomp, 0, NULL, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompressor_init_invalid_length: F=0 fails");

    /* F > max should fail */
    result = ccsds124_decompressor_init(&decomp, CCSDS124_MAX_PACKET_LENGTH + 1, NULL, 0);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompressor_init_invalid_length: F>max fails");
}

static void test_decompressor_init_invalid_robustness(void) {
    ccsds124_decompressor_t decomp;

    /* Robustness > 7 should fail */
    int result = ccsds124_decompressor_init(&decomp, 8, NULL, 8);
    TEST_ASSERT(result == CCSDS124_ERROR_INVALID_ARG, "test_decompressor_init_invalid_robustness: R=8 fails");
}

static void test_decompressor_init_with_mask(void) {
    ccsds124_decompressor_t decomp;
    bitvector_t initial_mask;

    bitvector_init(&initial_mask, 8);
    initial_mask.data[0] = 0xAB000000;

    int result = ccsds124_decompressor_init(&decomp, 8, &initial_mask, 1);

    TEST_ASSERT(result == CCSDS124_OK, "test_decompressor_init_with_mask: returns OK");
    TEST_ASSERT(decomp.mask.data[0] == 0xAB000000, "test_decompressor_init_with_mask: mask copied");
}

/* ============================================================================
 * Bit Reader Alignment Tests
 * ============================================================================ */

/**
 * @brief Test bitreader_align_byte() aligns to next byte boundary.
 */
static void test_bitreader_align_byte(void) {
    uint8_t data[2] = {0xFF, 0xAA};
    bitreader_t reader;
    bitreader_init(&reader, data, 16U);

    /* Read 3 bits to get mid-byte position */
    (void)bitreader_read_bit(&reader);
    (void)bitreader_read_bit(&reader);
    (void)bitreader_read_bit(&reader);
    TEST_ASSERT(bitreader_position(&reader) == 3U,
                "test_bitreader_align_byte: position is 3 after reading 3 bits");

    /* Align to next byte boundary */
    bitreader_align_byte(&reader);
    TEST_ASSERT(bitreader_position(&reader) == 8U,
                "test_bitreader_align_byte: position is 8 after align");

    /* Already at byte boundary — should not move */
    bitreader_align_byte(&reader);
    TEST_ASSERT(bitreader_position(&reader) == 8U,
                "test_bitreader_align_byte: position unchanged when already aligned");
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void) {
    printf("\nDecompression Tests\n");
    printf("===================\n\n");

    printf("Bit Reader Tests:\n");
    test_bitreader_init();
    test_bitreader_read_bit();
    test_bitreader_read_bits();
    test_bitreader_align_byte();

    printf("\nCOUNT Decoding Tests:\n");
    test_count_decode_1();
    test_count_decode_terminator();
    test_count_decode_2();
    test_count_decode_33();
    test_count_decode_34();
    test_count_roundtrip();

    printf("\nRLE Decoding Tests:\n");
    test_rle_decode_all_zeros();
    test_rle_roundtrip();

    printf("\nPacket Decompression Tests:\n");
    test_decompress_uncompressed_packet();
    test_decompress_roundtrip_multiple();

    printf("\nNULL Argument and Error Path Tests:\n");
    test_count_decode_null_args();
    test_count_decode_empty_reader();
    test_rle_decode_null_args();
    test_bit_insert_null_args();
    test_bit_insert_length_mismatch();
    test_decompressor_init_null();
    test_decompress_packet_null_args();
    test_decompress_null_args();
    test_decompress_output_overflow();

    printf("\nDecompressor Init Error Tests:\n");
    test_decompressor_init_invalid_length();
    test_decompressor_init_invalid_robustness();
    test_decompressor_init_with_mask();

    printf("\n%d/%d tests passed\n\n", tests_passed, tests_total);

    return (tests_passed == tests_total) ? 0 : 1;
}

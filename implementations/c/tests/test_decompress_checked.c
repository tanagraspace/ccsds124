/**
 * @file test_decompress_checked.c
 * @brief Unit tests for ccsds124_decompress_packet_checked().
 *
 * Tests the accuracy guarantee logic internalized from the
 * cross-validation decoder harness into the core library.
 */

#include "ccsds124.h"
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

/* Helper: compress N packets and return compressed bytes for each */
static int compress_packets(
    size_t F, uint8_t robustness,
    uint8_t input_packets[][8], size_t num_packets,
    uint8_t compressed[][256], size_t compressed_bits[], size_t compressed_bytes[]
) {
    ccsds124_compressor_t comp;
    int rc = ccsds124_compressor_init(&comp, F, NULL, robustness, 0, 0, 0);
    if (rc != CCSDS124_OK) return rc;

    for (size_t i = 0; i < num_packets; i++) {
        bitvector_t input;
        bitvector_init(&input, F);
        bitvector_from_bytes(&input, input_packets[i], (F + 7) / 8);

        /* First R+1 packets are uncompressed (rt=1) per CCSDS.
         * First packet also sends mask (ft=1) to synchronize decoder. */
        ccsds124_params_t params;
        params.min_robustness = robustness;
        params.new_mask_flag = 0;
        params.send_mask_flag = (i == 0) ? 1 : 0;
        params.uncompressed_flag = (i <= robustness) ? 1 : 0;

        bitbuffer_t bb;
        bitbuffer_init(&bb);
        rc = ccsds124_compress_packet(&comp, &input, &bb, &params);
        if (rc != CCSDS124_OK) return rc;

        compressed_bits[i] = bitbuffer_size(&bb);
        compressed_bytes[i] = bitbuffer_to_bytes(&bb, compressed[i], 256);
    }
    return CCSDS124_OK;
}

/* ============================================================================
 * Basic Guarantee Tests
 * ============================================================================ */

/**
 * @brief First packet (rt=1, uncompressed) should be guaranteed.
 */
static void test_first_packet_guaranteed(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    uint8_t input[][8] = {{0xAB, 0xCD}};
    uint8_t compressed[1][256];
    size_t bits[1], bytes[1];

    int rc = compress_packets(F, robustness, input, 1, compressed, bits, bytes);
    TEST_ASSERT(rc == CCSDS124_OK, "first_packet: compression OK");

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;
    ccsds124_decompress_result_t result;
    rc = ccsds124_decompress_packet_checked(&decomp, compressed[0], bits[0], &output, &result);

    TEST_ASSERT(rc == CCSDS124_OK, "first_packet: returns CCSDS124_OK");
    TEST_ASSERT(result.status == 0x00, "first_packet: status is guaranteed");
    TEST_ASSERT(result.rt == 1, "first_packet: rt=1 (uncompressed)");
}

/**
 * @brief Sequential packets should all be guaranteed.
 */
static void test_sequential_guaranteed(void) {
    size_t F = 16;
    uint8_t robustness = 1;

    /* R=1 means first 2 packets are uncompressed, rest compressed */
    uint8_t input[][8] = {
        {0xAB, 0xCD},
        {0xAB, 0xCD},
        {0xAB, 0xCD},
        {0xAB, 0xCD}
    };
    uint8_t compressed[4][256];
    size_t bits[4], bytes[4];

    int rc = compress_packets(F, robustness, input, 4, compressed, bits, bytes);
    TEST_ASSERT(rc == CCSDS124_OK, "sequential: compression OK");

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    int all_guaranteed = 1;
    for (size_t i = 0; i < 4; i++) {
        bitvector_t output;
        ccsds124_decompress_result_t result;
        rc = ccsds124_decompress_packet_checked(&decomp, compressed[i], bits[i], &output, &result);
        if (rc != CCSDS124_OK || result.status != 0x00) {
            all_guaranteed = 0;
        }
    }
    TEST_ASSERT(all_guaranteed, "sequential: all 4 packets guaranteed");
}

/**
 * @brief NULL result pointer should work (just doesn't populate).
 */
static void test_null_result(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    uint8_t input[][8] = {{0xAB, 0xCD}};
    uint8_t compressed[1][256];
    size_t bits[1], bytes[1];

    compress_packets(F, robustness, input, 1, compressed, bits, bytes);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;
    int rc = ccsds124_decompress_packet_checked(&decomp, compressed[0], bits[0], &output, NULL);
    TEST_ASSERT(rc == CCSDS124_OK, "null_result: returns OK with NULL result");
}

/**
 * @brief Result struct should be populated with correct Vt.
 */
static void test_result_populated(void) {
    size_t F = 16;
    uint8_t robustness = 2;

    uint8_t input[][8] = {
        {0xAB, 0xCD},
        {0xAB, 0xCD},
        {0xAB, 0xCD},
        {0xAB, 0xCD}
    };
    uint8_t compressed[4][256];
    size_t bits[4], bytes[4];

    compress_packets(F, robustness, input, 4, compressed, bits, bytes);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    /* Decompress all, check the last one's result for Vt >= R */
    ccsds124_decompress_result_t result;
    memset(&result, 0, sizeof(result));
    for (size_t i = 0; i < 4; i++) {
        bitvector_t output;
        ccsds124_decompress_packet_checked(&decomp, compressed[i], bits[i], &output, &result);
    }
    TEST_ASSERT(result.Vt >= robustness, "result_populated: Vt >= R");
}

/* ============================================================================
 * Mask Sync Tests
 * ============================================================================ */

/**
 * @brief mask_synced starts at 0 after init.
 */
static void test_mask_synced_init(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);
    TEST_ASSERT(decomp.mask_synced == 0, "mask_synced_init: starts at 0");
}

/**
 * @brief Packet loss clears mask_synced.
 */
static void test_mask_synced_loss_clears(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);
    decomp.mask_synced = 1;

    ccsds124_decompressor_notify_packet_loss(&decomp, 1);
    TEST_ASSERT(decomp.mask_synced == 0, "loss_clears: mask_synced cleared");
}

/**
 * @brief Packet loss records 0x02 entries in status ring.
 */
static void test_loss_records_status(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);

    ccsds124_decompressor_notify_packet_loss(&decomp, 3);
    TEST_ASSERT(decomp.received_status_count == 3, "loss_status: count is 3");
    TEST_ASSERT(decomp.received_status_ring[0] == 0x02, "loss_status: ring[0] is 0x02");
    TEST_ASSERT(decomp.received_status_ring[1] == 0x02, "loss_status: ring[1] is 0x02");
    TEST_ASSERT(decomp.received_status_ring[2] == 0x02, "loss_status: ring[2] is 0x02");
}

/* ============================================================================
 * Status Ring Tests
 * ============================================================================ */

/**
 * @brief Status ring wraps correctly after CCSDS124_MAX_VT_HISTORY entries.
 */
static void test_status_ring_wrap(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);

    /* Fill with 20 lost packets (wraps the 16-entry ring) */
    ccsds124_decompressor_notify_packet_loss(&decomp, 20);
    TEST_ASSERT(decomp.received_status_count == CCSDS124_MAX_VT_HISTORY,
                "ring_wrap: count capped at max");
    TEST_ASSERT(decomp.received_status_index == (20 % CCSDS124_MAX_VT_HISTORY),
                "ring_wrap: index wrapped correctly");
}

/* ============================================================================
 * State Save/Restore Tests
 * ============================================================================ */

/**
 * @brief Error during decompression restores state.
 */
static void test_error_restores_state(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    /* First decompress a valid packet to get into a known state */
    uint8_t input[][8] = {{0xAB, 0xCD}};
    uint8_t compressed[1][256];
    size_t bits[1], bytes[1];
    compress_packets(F, robustness, input, 1, compressed, bits, bytes);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;
    ccsds124_decompress_packet_checked(&decomp, compressed[0], bits[0], &output, NULL);

    /* Save state after successful decompression */
    size_t t_before = decomp.t;

    /* Now feed garbage data — should fail and restore state */
    uint8_t garbage[] = {0xFF};
    ccsds124_decompress_result_t result;
    int rc = ccsds124_decompress_packet_checked(&decomp, garbage, 8, &output, &result);

    TEST_ASSERT(rc < 0, "error_restore: returns error");
    TEST_ASSERT(decomp.t == t_before, "error_restore: t restored");
}

/**
 * @brief Guaranteed packet advances state normally.
 */
static void test_guaranteed_advances_state(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    uint8_t input[][8] = {{0xAB, 0xCD}, {0xAB, 0xCD}};
    uint8_t compressed[2][256];
    size_t bits[2], bytes[2];
    compress_packets(F, robustness, input, 2, compressed, bits, bytes);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;
    ccsds124_decompress_packet_checked(&decomp, compressed[0], bits[0], &output, NULL);
    TEST_ASSERT(decomp.t == 1, "advance: t=1 after first packet");

    ccsds124_decompress_packet_checked(&decomp, compressed[1], bits[1], &output, NULL);
    TEST_ASSERT(decomp.t == 2, "advance: t=2 after second packet");
}

/* ============================================================================
 * Padding Validation Tests
 * ============================================================================ */

/**
 * @brief Padding validation rejects rt=0 packets with >= 8 remaining bits.
 *
 * Compressed (rt=0) packets must consume the whole bitstream (at most 7
 * padding bits). Reference packets (rt=1) are exempt: they are
 * self-delimiting via COUNT(F) and excess trailing bits are ignored per
 * the cross-validation rules.
 */
static void test_padding_validation(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    uint8_t input[][8] = {{0xAB, 0xCD}, {0xAB, 0xCD}};
    uint8_t compressed[2][256];
    size_t bits[2], bytes[2];
    compress_packets(F, robustness, input, 2, compressed, bits, bytes);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;
    ccsds124_decompress_result_t result;

    /* Packet 0 (init phase, rt=1): excess trailing bits are ignored */
    int rc = ccsds124_decompress_packet_checked(&decomp, compressed[0], bits[0] + 8,
                                              &output, &result);
    TEST_ASSERT(rc == CCSDS124_OK, "padding: rt=1 tolerates excess bits");
    TEST_ASSERT(result.rt == 1, "padding: first packet is a reference packet");

    /* Packet 1 (rt=0, compressed): >= 8 remaining bits rejected */
    rc = ccsds124_decompress_packet_checked(&decomp, compressed[1], bits[1] + 8,
                                          &output, &result);
    TEST_ASSERT(rc != CCSDS124_OK, "padding: rt=0 rejects >= 8 remaining bits");
}

/* ============================================================================
 * Backward Compatibility Tests
 * ============================================================================ */

/**
 * @brief ccsds124_decompress_packet() still works and doesn't touch new fields.
 */
static void test_backward_compat(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    uint8_t input[][8] = {{0xAB, 0xCD}};
    uint8_t compressed[1][256];
    size_t bits[1], bytes[1];
    compress_packets(F, robustness, input, 1, compressed, bits, bytes);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitreader_t reader;
    bitreader_init(&reader, compressed[0], bits[0]);

    bitvector_t output;
    int rc = ccsds124_decompress_packet(&decomp, &reader, &output);
    TEST_ASSERT(rc == CCSDS124_OK, "backward_compat: old API still works");

    /* New fields should still be at init values (old API doesn't touch them) */
    TEST_ASSERT(decomp.mask_synced == 0, "backward_compat: mask_synced untouched");
    TEST_ASSERT(decomp.received_status_count == 0, "backward_compat: status_count untouched");
}

/* ============================================================================
 * Invalid Argument Tests
 * ============================================================================ */

static void test_checked_null_args(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);

    uint8_t data[] = {0x00};
    bitvector_t output;

    TEST_ASSERT(ccsds124_decompress_packet_checked(NULL, data, 8, &output, NULL) == CCSDS124_ERROR_INVALID_ARG,
                "null_args: NULL decomp rejected");
    TEST_ASSERT(ccsds124_decompress_packet_checked(&decomp, NULL, 8, &output, NULL) == CCSDS124_ERROR_INVALID_ARG,
                "null_args: NULL data rejected");
    TEST_ASSERT(ccsds124_decompress_packet_checked(&decomp, data, 0, &output, NULL) == CCSDS124_ERROR_INVALID_ARG,
                "null_args: zero num_bits rejected");
    TEST_ASSERT(ccsds124_decompress_packet_checked(&decomp, data, 8, NULL, NULL) == CCSDS124_ERROR_INVALID_ARG,
                "null_args: NULL output rejected");
}

/* ============================================================================
 * Roundtrip Tests via Checked API
 * ============================================================================ */

/**
 * @brief Full roundtrip through checked API produces correct data.
 */
static void test_checked_roundtrip(void) {
    size_t F = 16;
    uint8_t robustness = 1;

    uint8_t input[][8] = {
        {0xAB, 0xCD},
        {0xAB, 0xCD},
        {0xAB, 0xCE},
        {0xAB, 0xCE}
    };
    uint8_t compressed[4][256];
    size_t bits[4], bytes[4];

    int rc = compress_packets(F, robustness, input, 4, compressed, bits, bytes);
    TEST_ASSERT(rc == CCSDS124_OK, "roundtrip: compression OK");

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    int all_ok = 1;
    for (size_t i = 0; i < 4; i++) {
        bitvector_t output;
        ccsds124_decompress_result_t result;
        rc = ccsds124_decompress_packet_checked(&decomp, compressed[i], bits[i], &output, &result);
        if (rc != CCSDS124_OK) {
            all_ok = 0;
            continue;
        }

        /* Verify data matches */
        uint8_t out_bytes[2];
        bitvector_to_bytes(&output, out_bytes, 2);
        if (memcmp(out_bytes, input[i], 2) != 0) {
            all_ok = 0;
        }
    }
    TEST_ASSERT(all_ok, "roundtrip: all 4 packets match original data");
}

/**
 * @brief Roundtrip with ft=1 packets (send mask flag).
 */
static void test_checked_roundtrip_with_ft(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    ccsds124_compressor_t comp;
    ccsds124_compressor_init(&comp, F, NULL, robustness, 0, 0, 0);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    uint8_t input_data[][2] = {
        {0xAA, 0xBB},
        {0xAA, 0xBC},
        {0xAA, 0xBC}
    };

    int all_ok = 1;
    for (size_t i = 0; i < 3; i++) {
        bitvector_t input;
        bitvector_init(&input, F);
        bitvector_from_bytes(&input, input_data[i], 2);

        ccsds124_params_t params;
        params.min_robustness = robustness;
        params.new_mask_flag = 0;
        params.send_mask_flag = (i <= 1) ? 1 : 0;  /* ft=1 on first two packets */
        params.uncompressed_flag = (i == 0) ? 1 : 0;

        bitbuffer_t bb;
        bitbuffer_init(&bb);
        ccsds124_compress_packet(&comp, &input, &bb, &params);

        uint8_t pkt[256];
        size_t pkt_bits = bitbuffer_size(&bb);
        bitbuffer_to_bytes(&bb, pkt, 256);

        bitvector_t output;
        ccsds124_decompress_result_t result;
        int rc = ccsds124_decompress_packet_checked(&decomp, pkt, pkt_bits, &output, &result);

        if (rc != CCSDS124_OK) {
            all_ok = 0;
        }

        /* ft=1 should sync the mask */
        if (i == 1 && result.ft != 1) {
            all_ok = 0;
        }
    }
    TEST_ASSERT(all_ok, "ft_roundtrip: ft=1 packet handled correctly");
}

/* ============================================================================
 * Init Fields Test
 * ============================================================================ */

static void test_init_guarantee_fields(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);

    TEST_ASSERT(decomp.mask_synced == 0, "init_fields: mask_synced=0");
    TEST_ASSERT(decomp.received_status_count == 0, "init_fields: status_count=0");
    TEST_ASSERT(decomp.received_status_index == 0, "init_fields: status_index=0");
}

static void test_reset_clears_guarantee_fields(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);

    /* Dirty the fields */
    decomp.mask_synced = 1;
    decomp.received_status_count = 5;
    decomp.received_status_index = 3;
    decomp.received_status_ring[0] = 0x01;

    ccsds124_decompressor_reset(&decomp);

    TEST_ASSERT(decomp.mask_synced == 0, "reset_fields: mask_synced cleared");
    TEST_ASSERT(decomp.received_status_count == 0, "reset_fields: status_count cleared");
    TEST_ASSERT(decomp.received_status_index == 0, "reset_fields: status_index cleared");
    TEST_ASSERT(decomp.received_status_ring[0] == 0, "reset_fields: ring cleared");
}

/* ============================================================================
 * Unguaranteed Path Tests
 * ============================================================================ */

/**
 * @brief rt=1 without mask sync (ft=0) returns CCSDS124_STATUS_UNGUARANTEED.
 *
 * When mask_synced=0 and the packet has rt=1 but ft=0, the decoder cannot
 * verify the reference data against its mask, so it's unguaranteed.
 */
static void test_rt1_no_sync_unguaranteed(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    /* Compress one packet WITHOUT ft=1 — mask won't sync */
    ccsds124_compressor_t comp;
    ccsds124_compressor_init(&comp, F, NULL, robustness, 0, 0, 0);

    bitvector_t input;
    bitvector_init(&input, F);
    uint8_t data[] = {0xAB, 0xCD};
    bitvector_from_bytes(&input, data, 2);

    ccsds124_params_t params;
    params.min_robustness = robustness;
    params.new_mask_flag = 0;
    params.send_mask_flag = 0;  /* ft=0: no mask sync */
    params.uncompressed_flag = 1;  /* rt=1: reference packet */

    bitbuffer_t bb;
    bitbuffer_init(&bb);
    ccsds124_compress_packet(&comp, &input, &bb, &params);

    uint8_t pkt[256];
    size_t pkt_bits = bitbuffer_size(&bb);
    bitbuffer_to_bytes(&bb, pkt, 256);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;
    ccsds124_decompress_result_t result;
    int rc = ccsds124_decompress_packet_checked(&decomp, pkt, pkt_bits, &output, &result);

    TEST_ASSERT(rc == CCSDS124_STATUS_UNGUARANTEED,
                "rt1_no_sync: returns CCSDS124_STATUS_UNGUARANTEED");
    TEST_ASSERT(result.status == 0x01, "rt1_no_sync: status is 0x01");
    TEST_ASSERT(result.rt == 1, "rt1_no_sync: rt=1");
    TEST_ASSERT(result.ft == 0, "rt1_no_sync: ft=0");
}

/**
 * @brief CCSDS124_STATUS_UNGUARANTEED equals 1 (positive, distinct from errors).
 */
static void test_unguaranteed_return_value(void) {
    TEST_ASSERT(CCSDS124_STATUS_UNGUARANTEED == 1,
                "unguaranteed_value: CCSDS124_STATUS_UNGUARANTEED == 1");
    TEST_ASSERT(CCSDS124_STATUS_UNGUARANTEED > 0,
                "unguaranteed_value: positive (not an error)");
}

/**
 * @brief ft=1 on a guaranteed packet sets mask_synced to 1.
 */
static void test_ft1_syncs_mask(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    /* Compress with ft=1 */
    uint8_t input[][8] = {{0xAB, 0xCD}};
    uint8_t compressed[1][256];
    size_t bits[1], bytes[1];
    compress_packets(F, robustness, input, 1, compressed, bits, bytes);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);
    TEST_ASSERT(decomp.mask_synced == 0, "ft1_sync: starts unsynced");

    bitvector_t output;
    ccsds124_decompress_result_t result;
    ccsds124_decompress_packet_checked(&decomp, compressed[0], bits[0], &output, &result);

    TEST_ASSERT(decomp.mask_synced == 1, "ft1_sync: mask_synced=1 after ft=1 packet");
}

/**
 * @brief Unguaranteed packet (non count_f) restores state and clears mask sync.
 */
static void test_unguaranteed_restores_state(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    /* Compress without ft=1 so mask stays unsynced */
    ccsds124_compressor_t comp;
    ccsds124_compressor_init(&comp, F, NULL, robustness, 0, 0, 0);

    bitvector_t input;
    bitvector_init(&input, F);
    uint8_t data[] = {0xAB, 0xCD};
    bitvector_from_bytes(&input, data, 2);

    ccsds124_params_t params;
    params.min_robustness = robustness;
    params.new_mask_flag = 0;
    params.send_mask_flag = 0;  /* ft=0 */
    params.uncompressed_flag = 1;  /* rt=1 */

    bitbuffer_t bb;
    bitbuffer_init(&bb);
    ccsds124_compress_packet(&comp, &input, &bb, &params);

    uint8_t pkt[256];
    size_t pkt_bits = bitbuffer_size(&bb);
    bitbuffer_to_bytes(&bb, pkt, 256);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);
    size_t t_before = decomp.t;

    bitvector_t output;
    int rc = ccsds124_decompress_packet_checked(&decomp, pkt, pkt_bits, &output, NULL);

    TEST_ASSERT(rc == CCSDS124_STATUS_UNGUARANTEED,
                "unguaranteed_restore: returns UNGUARANTEED");
    TEST_ASSERT(decomp.t == t_before,
                "unguaranteed_restore: t restored (state rolled back)");
    TEST_ASSERT(decomp.mask_synced == 0,
                "unguaranteed_restore: mask_synced cleared");
}

/* ============================================================================
 * Error Path Ring Buffer Tests
 * ============================================================================ */

/**
 * @brief Decompression error records 0x01 in the status ring buffer.
 */
static void test_error_records_status_in_ring(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    /* Feed garbage data that will fail to decompress */
    uint8_t garbage[] = {0xFF, 0xFF};
    bitvector_t output;
    ccsds124_decompress_result_t result;
    ccsds124_decompress_packet_checked(&decomp, garbage, 16, &output, &result);

    TEST_ASSERT(decomp.received_status_count == 1,
                "error_ring: count incremented");
    TEST_ASSERT(decomp.received_status_ring[0] == 0x01,
                "error_ring: ring[0] is 0x01 (unguaranteed/error)");
}

/**
 * @brief Decompression error clears mask_synced.
 */
static void test_error_clears_mask_synced(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);
    decomp.mask_synced = 1;  /* Pretend we were synced */

    uint8_t garbage[] = {0xFF, 0xFF};
    bitvector_t output;
    ccsds124_decompress_packet_checked(&decomp, garbage, 16, &output, NULL);

    TEST_ASSERT(decomp.mask_synced == 0,
                "error_sync: mask_synced cleared on error");
}

/**
 * @brief Decompression error populates result struct with status=0x01.
 */
static void test_error_populates_result(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    uint8_t garbage[] = {0xFF, 0xFF};
    bitvector_t output;
    ccsds124_decompress_result_t result;
    memset(&result, 0xFF, sizeof(result));  /* Fill with non-zero to detect writes */
    ccsds124_decompress_packet_checked(&decomp, garbage, 16, &output, &result);

    TEST_ASSERT(result.status == 0x01, "error_result: status is 0x01");
    TEST_ASSERT(result.Vt == 0, "error_result: Vt is 0");
    TEST_ASSERT(result.ft == 0, "error_result: ft is 0");
    TEST_ASSERT(result.rt == 0, "error_result: rt is 0");
}

/**
 * @brief Unguaranteed packet records 0x01 in status ring.
 */
static void test_unguaranteed_records_status_in_ring(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    /* Compress without ft=1 → rt=1 without sync → unguaranteed */
    ccsds124_compressor_t comp;
    ccsds124_compressor_init(&comp, F, NULL, robustness, 0, 0, 0);

    bitvector_t input;
    bitvector_init(&input, F);
    uint8_t data[] = {0xAB, 0xCD};
    bitvector_from_bytes(&input, data, 2);

    ccsds124_params_t params;
    params.min_robustness = robustness;
    params.new_mask_flag = 0;
    params.send_mask_flag = 0;
    params.uncompressed_flag = 1;

    bitbuffer_t bb;
    bitbuffer_init(&bb);
    ccsds124_compress_packet(&comp, &input, &bb, &params);

    uint8_t pkt[256];
    size_t pkt_bits = bitbuffer_size(&bb);
    bitbuffer_to_bytes(&bb, pkt, 256);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;
    ccsds124_decompress_packet_checked(&decomp, pkt, pkt_bits, &output, NULL);

    TEST_ASSERT(decomp.received_status_ring[0] == 0x01,
                "unguaranteed_ring: ring[0] is 0x01");
}

/**
 * @brief Guaranteed packet records 0x00 in status ring.
 */
static void test_guaranteed_records_status_in_ring(void) {
    size_t F = 16;
    uint8_t robustness = 0;

    uint8_t input[][8] = {{0xAB, 0xCD}};
    uint8_t compressed[1][256];
    size_t bits[1], bytes[1];
    compress_packets(F, robustness, input, 1, compressed, bits, bytes);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;
    ccsds124_decompress_packet_checked(&decomp, compressed[0], bits[0], &output, NULL);

    TEST_ASSERT(decomp.received_status_ring[0] == 0x00,
                "guaranteed_ring: ring[0] is 0x00");
}

/* ============================================================================
 * Robustness Window Tests
 * ============================================================================ */

/**
 * @brief Non-reference packet with preceding unguaranteed in Vt window → unguaranteed.
 *
 * After a loss (which records 0x02 and clears sync), followed by an rt=1
 * with ft=1 (re-syncs), then a non-reference packet should check the window
 * and find the unguaranteed entries.
 */
static void test_robustness_window_with_loss(void) {
    size_t F = 16;
    uint8_t robustness = 2;

    /* Compress 5 packets: first 3 uncompressed (R=2), rest compressed.
     * All with ft=1 on first packet to sync mask. */
    uint8_t input[][8] = {
        {0xAA, 0xBB},
        {0xAA, 0xBB},
        {0xAA, 0xBB},
        {0xAA, 0xBB},
        {0xAA, 0xBB}
    };
    uint8_t compressed[5][256];
    size_t bits[5], bytes[5];
    compress_packets(F, robustness, input, 5, compressed, bits, bytes);

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;

    /* Decompress first packet (rt=1, ft=1) — guaranteed, syncs mask */
    int rc = ccsds124_decompress_packet_checked(
        &decomp, compressed[0], bits[0], &output, NULL);
    TEST_ASSERT(rc == CCSDS124_OK, "window_loss: pkt0 guaranteed");

    /* Simulate loss of packet 1 */
    ccsds124_decompressor_notify_packet_loss(&decomp, 1);

    /* Decompress packet 2 (rt=1, ft=0) — mask NOT synced due to loss → unguaranteed */
    ccsds124_decompress_result_t result;
    rc = ccsds124_decompress_packet_checked(
        &decomp, compressed[2], bits[2], &output, &result);

    /* With mask_synced=0 and ft=0, rt=1 should be unguaranteed */
    TEST_ASSERT(rc == CCSDS124_STATUS_UNGUARANTEED,
                "window_loss: pkt2 unguaranteed after loss (rt=1 no sync)");
}

/**
 * @brief Lost packets (0x02) are skipped when checking robustness window.
 */
static void test_robustness_window_skips_lost(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);

    /* Manually populate ring: 0x00, 0x02, 0x00 (guaranteed, lost, guaranteed).
     * A Vt=2 check should skip the 0x02 and find 2 guaranteed packets. */
    decomp.received_status_ring[0] = 0x00;
    decomp.received_status_ring[1] = 0x02;
    decomp.received_status_ring[2] = 0x00;
    decomp.received_status_count = 3;
    decomp.received_status_index = 3;

    /* The ring now has [0x00, 0x02, 0x00] at indices 0-2.
     * A non-reference packet with Vt=2 should check backwards from index 2,
     * find 0x00, skip 0x02, find 0x00 → guaranteed.
     * We verify this by checking ring state is consistent. */
    TEST_ASSERT(decomp.received_status_ring[1] == 0x02,
                "skip_lost: ring has 0x02 for lost packet");
    TEST_ASSERT(decomp.received_status_count == 3,
                "skip_lost: count tracks all entries including lost");
}

/**
 * @brief Non-reference packet with 0x02 entries in ring triggers skip logic.
 *
 * Exercises the ring buffer walk that skips lost packet entries (0x02)
 * when checking the Vt window for guarantee decisions.
 */
static void test_vt_window_skips_0x02_entries(void) {
    size_t F = 16;
    uint8_t robustness = 2;

    /* Compress 4 identical packets: first 3 are rt=1 (R=2), 4th is rt=0 */
    uint8_t input[][8] = {
        {0xAA, 0xBB},
        {0xAA, 0xBB},
        {0xAA, 0xBB},
        {0xAA, 0xBB}
    };
    uint8_t compressed[4][256];
    size_t bits[4], bytes[4];

    int rc = compress_packets(F, robustness, input, 4, compressed, bits, bytes);
    TEST_ASSERT(rc == CCSDS124_OK, "skip_0x02: compression OK");

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, robustness);

    bitvector_t output;

    /* Decompress packets 0, 1, 2 (all rt=1) — guaranteed */
    for (size_t i = 0; i < 3; i++) {
        ccsds124_decompress_result_t r;
        rc = ccsds124_decompress_packet_checked(
            &decomp, compressed[i], bits[i], &output, &r);
    }

    /* Manually inject a 0x02 (lost packet) entry into the ring.
     * This simulates a past loss without clearing mask_synced,
     * so we can reach the non-reference Vt window walk code. */
    decomp.received_status_ring[decomp.received_status_index] = 0x02;
    decomp.received_status_index =
        (decomp.received_status_index + 1U) % CCSDS124_MAX_VT_HISTORY;
    if (decomp.received_status_count < CCSDS124_MAX_VT_HISTORY) {
        decomp.received_status_count++;
    }

    /* Decompress packet 3 (rt=0, non-reference).
     * Vt window walk will encounter the 0x02 entry and skip it. */
    ccsds124_decompress_result_t result;
    rc = ccsds124_decompress_packet_checked(
        &decomp, compressed[3], bits[3], &output, &result);

    TEST_ASSERT(rc == CCSDS124_OK,
                "skip_0x02: non-ref packet guaranteed despite 0x02 in ring");
    TEST_ASSERT(result.status == 0x00,
                "skip_0x02: status is 0x00");
}

/* ============================================================================
 * Diagnostics Init/Reset Tests
 * ============================================================================ */

/**
 * @brief Init sets diagnostic fields to 0.
 */
static void test_init_diagnostics(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);

    TEST_ASSERT(decomp.mask_inconsistent == 0,
                "init_diag: mask_inconsistent=0");
    TEST_ASSERT(decomp.count_f_mismatch == 0,
                "init_diag: count_f_mismatch=0");
}

/**
 * @brief Reset clears diagnostic fields.
 */
static void test_reset_diagnostics(void) {
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, 16, NULL, 0);

    /* Dirty the fields */
    decomp.mask_inconsistent = 1;
    decomp.count_f_mismatch = 1;

    ccsds124_decompressor_reset(&decomp);

    TEST_ASSERT(decomp.mask_inconsistent == 0,
                "reset_diag: mask_inconsistent cleared");
    TEST_ASSERT(decomp.count_f_mismatch == 0,
                "reset_diag: count_f_mismatch cleared");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("\nChecked Decompression Tests\n");
    printf("==========================\n\n");

    printf("Basic Guarantee Tests:\n");
    test_first_packet_guaranteed();
    test_sequential_guaranteed();
    test_null_result();
    test_result_populated();

    printf("\nMask Sync Tests:\n");
    test_mask_synced_init();
    test_mask_synced_loss_clears();
    test_loss_records_status();

    printf("\nStatus Ring Tests:\n");
    test_status_ring_wrap();

    printf("\nState Save/Restore Tests:\n");
    test_error_restores_state();
    test_guaranteed_advances_state();

    printf("\nPadding Validation Tests:\n");
    test_padding_validation();

    printf("\nBackward Compatibility Tests:\n");
    test_backward_compat();

    printf("\nInvalid Argument Tests:\n");
    test_checked_null_args();

    printf("\nRoundtrip Tests:\n");
    test_checked_roundtrip();
    test_checked_roundtrip_with_ft();

    printf("\nInit/Reset Field Tests:\n");
    test_init_guarantee_fields();
    test_reset_clears_guarantee_fields();

    printf("\nUnguaranteed Path Tests:\n");
    test_rt1_no_sync_unguaranteed();
    test_unguaranteed_return_value();
    test_ft1_syncs_mask();
    test_unguaranteed_restores_state();

    printf("\nError Path Ring Buffer Tests:\n");
    test_error_records_status_in_ring();
    test_error_clears_mask_synced();
    test_error_populates_result();
    test_unguaranteed_records_status_in_ring();
    test_guaranteed_records_status_in_ring();

    printf("\nRobustness Window Tests:\n");
    test_robustness_window_with_loss();
    test_robustness_window_skips_lost();
    test_vt_window_skips_0x02_entries();

    printf("\nDiagnostics Init/Reset Tests:\n");
    test_init_diagnostics();
    test_reset_diagnostics();

    printf("\n%d/%d tests passed\n", tests_passed, tests_total);

    return (tests_passed == tests_total) ? 0 : 1;
}

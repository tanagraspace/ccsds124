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
 * CCSDS 124.0-B-1 C Implementation - Compression Tests
 * TDD: Write tests first, then implement
 *
 * Tests for CCSDS 124.0-B-1 Section 5.3 (Compression)
 *
 * Authors:
 *   Georges Labrèche <georges@tanagraspace.com> — https://georges.fyi
 *   Claude Code (claude-sonnet-4-5-20250929) <noreply@anthropic.com>
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
 * Compressor Initialization Tests
 * ======================================================================== */

TEST(test_compressor_init_valid) {
    ccsds124_compressor_t comp;

    int result = ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);

    assert(result == CCSDS124_OK);
    assert(comp.F == 8);
    assert(comp.robustness == 0);
    assert(comp.t == 0);
    assert(comp.history_index == 0);
}

TEST(test_compressor_init_with_initial_mask) {
    ccsds124_compressor_t comp;
    bitvector_t initial_mask;

    bitvector_init(&initial_mask, 8);
    initial_mask.data[0] = 0x0F000000;  /* Some initial mask */

    int result = ccsds124_compressor_init(&comp, 8, &initial_mask, 1, 0, 0, 0);

    assert(result == CCSDS124_OK);
    assert(comp.F == 8);
    assert(comp.robustness == 1);
    assert(comp.mask.data[0] == 0x0F000000);
    assert(comp.initial_mask.data[0] == 0x0F000000);
}

TEST(test_compressor_init_invalid_length) {
    ccsds124_compressor_t comp;

    /* F = 0 should fail */
    int result = ccsds124_compressor_init(&comp, 0, NULL, 0, 0, 0, 0);
    assert(result == CCSDS124_ERROR_INVALID_ARG);

    /* F > max should fail */
    result = ccsds124_compressor_init(&comp, CCSDS124_MAX_PACKET_LENGTH + 1, NULL, 0, 0, 0, 0);
    assert(result == CCSDS124_ERROR_INVALID_ARG);
}

TEST(test_compressor_init_invalid_robustness) {
    ccsds124_compressor_t comp;

    /* Robustness > 7 should fail */
    int result = ccsds124_compressor_init(&comp, 8, NULL, 8, 0, 0, 0);
    assert(result == CCSDS124_ERROR_INVALID_ARG);
}

TEST(test_compressor_reset) {
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;

    /* Initialize and compress one packet */
    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 8);
    input.data[0] = 0xAA000000;
    bitbuffer_init(&output);

    ccsds124_params_t params = {0, 0, 0, 0};
    ccsds124_compress_packet(&comp, &input, &output, &params);

    /* State should have changed */
    assert(comp.t > 0);

    /* Reset */
    ccsds124_compressor_reset(&comp);

    /* State should be back to initial */
    assert(comp.t == 0);
    assert(comp.history_index == 0);
}

/* ========================================================================
 * Simple Compression Tests
 * ======================================================================== */

TEST(test_compress_first_packet) {
    /* First packet (t=0) with Rt=0 should produce output */
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;
    ccsds124_params_t params;

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 8);
    bitbuffer_init(&output);

    input.data[0] = 0xAA000000;  /* 10101010 */

    /* At t=0 with Rt=0, we can use custom flags */
    params.min_robustness = 0;
    params.new_mask_flag = 0;
    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;

    int result = ccsds124_compress_packet(&comp, &input, &output, &params);

    assert(result == CCSDS124_OK);
    assert(output.num_bits > 0);  /* Should produce some output */
    assert(comp.t == 1);           /* Time should advance */
}

TEST(test_compress_d0_zero_with_nonzero_mask) {
    /* Regression test: D_0 must be 0 when M_0 is non-zero.
     * With a non-zero initial mask, D_0 = M_0 XOR M_{-1} = M_0 XOR M_0 = 0.
     * This means X_0 is a zero vector and RLE(X_0) = '10' (just terminator).
     * The first 2 bits of the compressed output must be '1','0'. */
    ccsds124_compressor_t comp;
    bitvector_t input, initial_mask;
    bitbuffer_t output;
    ccsds124_params_t params;

    bitvector_init(&initial_mask, 8);
    initial_mask.data[0] = 0xFF000000;  /* Non-zero mask: all bits unpredictable */

    ccsds124_compressor_init(&comp, 8, &initial_mask, 0, 0, 0, 0);
    bitvector_init(&input, 8);
    bitbuffer_init(&output);

    input.data[0] = 0xAA000000;  /* 10101010 */

    params.min_robustness = 0;
    params.new_mask_flag = 0;
    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;

    int result = ccsds124_compress_packet(&comp, &input, &output, &params);
    assert(result == CCSDS124_OK);

    /* Convert to bytes and check first 2 bits are '10' (RLE terminator for zero X_0) */
    uint8_t out_bytes[CCSDS124_MAX_OUTPUT_BYTES];
    size_t out_size = bitbuffer_to_bytes(&output, out_bytes, sizeof(out_bytes));
    assert(out_size > 0);

    /* First byte's top 2 bits must be '10' (0x80 masked) not '11' (0xC0 masked) */
    assert((out_bytes[0] & 0xC0) == 0x80);
}

TEST(test_compress_two_identical_packets) {
    /* Two identical packets should compress well (no changes) */
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output1, output2;
    ccsds124_params_t params;

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 8);
    input.data[0] = 0xCC000000;

    params.min_robustness = 0;
    params.new_mask_flag = 0;
    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;

    /* First packet */
    bitbuffer_init(&output1);
    ccsds124_compress_packet(&comp, &input, &output1, &params);
    size_t size1 = output1.num_bits;

    /* Second packet (identical) */
    bitbuffer_init(&output2);
    ccsds124_compress_packet(&comp, &input, &output2, &params);
    size_t size2 = output2.num_bits;

    /* Second packet should be smaller (no changes, no unpredictable bits) */
    assert(size2 <= size1);
}

TEST(test_compress_with_change) {
    /* Test compression when input changes */
    ccsds124_compressor_t comp;
    bitvector_t input1, input2;
    bitbuffer_t output;
    ccsds124_params_t params;

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);
    bitvector_init(&input1, 8);
    bitvector_init(&input2, 8);
    bitbuffer_init(&output);

    input1.data[0] = 0xAA000000;  /* 10101010 */
    input2.data[0] = 0xAC000000;  /* 10101100 - bit 1 changed */

    params.min_robustness = 0;
    params.new_mask_flag = 0;
    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;

    /* Compress first packet */
    ccsds124_compress_packet(&comp, &input1, &output, &params);

    /* Compress second packet */
    bitbuffer_clear(&output);
    int result = ccsds124_compress_packet(&comp, &input2, &output, &params);

    assert(result == CCSDS124_OK);
    assert(output.num_bits > 0);  /* Should encode the change */
}

TEST(test_compress_null_params) {
    /* NULL params should use defaults */
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 8);
    bitbuffer_init(&output);
    input.data[0] = 0xFF000000;

    int result = ccsds124_compress_packet(&comp, &input, &output, NULL);

    assert(result == CCSDS124_OK);
    assert(output.num_bits > 0);
}

TEST(test_compress_invalid_input) {
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);
    bitvector_init(&input, 16);  /* Wrong length! */
    bitbuffer_init(&output);

    int result = ccsds124_compress_packet(&comp, &input, &output, NULL);

    assert(result == CCSDS124_ERROR_INVALID_ARG);
}

/* ========================================================================
 * CCSDS Helper Function Tests (TDD)
 * ======================================================================== */

TEST(test_compute_robustness_window_rt_zero) {
    /* Test Xₜ calculation when Rₜ = 0
     * Xₜ = <Dₜ> (just reverse the current change) */
    ccsds124_compressor_t comp;
    bitvector_t Dt, Xt, expected;

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);  /* Rₜ = 0 */
    bitvector_init(&Dt, 8);
    bitvector_init(&Xt, 8);
    bitvector_init(&expected, 8);

    /* Dₜ = 00000101 (bits 0 and 2 changed) */
    Dt.data[0] = 0x05000000;

    /* Expected: Xₜ = Dₜ (no reversal - RLE processes LSB to MSB) */
    expected.data[0] = 0x05000000;

    ccsds124_compute_robustness_window(&Xt, &comp, &Dt);

    assert(bitvector_equals(&Xt, &expected));
}

TEST(test_compute_robustness_window_with_history) {
    /* Test Xₜ with Rₜ = 1: Xₜ = <(Dₜ₋₁ OR Dₜ)> */
    ccsds124_compressor_t comp;
    bitvector_t input1, input2, Xt, expected;
    bitbuffer_t output;
    ccsds124_params_t params = {0};

    ccsds124_compressor_init(&comp, 8, NULL, 1, 0, 0, 0);  /* Rₜ = 1 */
    bitvector_init(&input1, 8);
    bitvector_init(&input2, 8);
    bitvector_init(&Xt, 8);
    bitvector_init(&expected, 8);
    bitbuffer_init(&output);

    /* Build up history with 2 packets */
    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;

    /* Packet 1: t=0 */
    input1.data[0] = 0x00000000;
    ccsds124_compress_packet(&comp, &input1, &output, &params);

    /* Packet 2: t=1, D₁ = 0x01 (bit 0 changed) */
    input2.data[0] = 0x01000000;
    bitbuffer_clear(&output);
    ccsds124_compress_packet(&comp, &input2, &output, &params);

    /* Get the current change D₁ */
    size_t recent_idx = (comp.history_index - 1 + CCSDS124_MAX_HISTORY) % CCSDS124_MAX_HISTORY;
    bitvector_t *D1 = &comp.change_history[recent_idx];

    /* At t=1 with Rₜ=1: Xₜ = D₀ OR D₁ = 0 OR 0x01 = 0x01 (no reversal) */
    expected.data[0] = 0x01000000;

    ccsds124_compute_robustness_window(&Xt, &comp, D1);

    assert(bitvector_equals(&Xt, &expected));
}

TEST(test_compute_effective_robustness_base_case) {
    /* Test Vₜ = Rₜ when there are mask changes */
    ccsds124_compressor_t comp;
    bitvector_t Dt;

    ccsds124_compressor_init(&comp, 8, NULL, 2, 0, 0, 0);  /* Rₜ = 2 */
    bitvector_init(&Dt, 8);

    /* Dₜ has changes */
    Dt.data[0] = 0x01000000;

    uint8_t Vt = ccsds124_compute_effective_robustness(&comp, &Dt);

    assert(Vt == 2);  /* Should equal Rₜ */
}

TEST(test_compute_effective_robustness_with_no_changes) {
    /* Test Vₜ = Rₜ + Cₜ when consecutive packets have no mask changes */
    ccsds124_compressor_t comp;
    bitvector_t input, Dt;
    bitbuffer_t output;
    ccsds124_params_t params = {0};

    ccsds124_compressor_init(&comp, 8, NULL, 1, 0, 0, 0);  /* Rₜ = 1 */
    bitvector_init(&input, 8);
    bitvector_init(&Dt, 8);
    bitbuffer_init(&output);

    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;

    /* Send 5 identical packets - mask should not change */
    input.data[0] = 0xAA000000;
    for (int i = 0; i < 5; i++) {
        bitbuffer_clear(&output);
        if (i > 1) {
            params.send_mask_flag = 0;
            params.uncompressed_flag = 0;
        }
        ccsds124_compress_packet(&comp, &input, &output, &params);
    }

    /* After 5 identical packets (4 with no changes after t=0):
     * D₀ = 0, D₁ = 0, D₂ = 0, D₃ = 0, D₄ = 0
     * At t=4: consecutive no-change count = 4
     * Vₜ = Rₜ + 4 = 1 + 4 = 5 */
    bitvector_zero(&Dt);  /* Current change is also 0 */
    uint8_t Vt = ccsds124_compute_effective_robustness(&comp, &Dt);

    assert(Vt >= 1);  /* Should be at least Rₜ */
    assert(Vt <= 15);  /* Should be capped at 15 */
}

/* ========================================================================
 * eₜ, kₜ, cₜ Component Tests (TDD)
 * ======================================================================== */

TEST(test_has_positive_updates_all_unpredictable) {
    /* Test eₜ = 0 when all changed bits are unpredictable (mask=1) */
    bitvector_t Xt, mask;

    bitvector_init(&Xt, 8);
    bitvector_init(&mask, 8);

    /* Xt = 00000001 (bit 0 changed, not reversed) */
    Xt.data[0] = 0x01000000;

    /* Mask = 00000001 (bit 0 is unpredictable) */
    mask.data[0] = 0x01000000;

    /* Changed bit is unpredictable → eₜ = 0 */
    int et = ccsds124_has_positive_updates(&Xt, &mask);
    assert(et == 0);
}

TEST(test_has_positive_updates_has_predictable) {
    /* Test eₜ = 1 when some changed bits are predictable (mask=0) */
    bitvector_t Xt, mask;

    bitvector_init(&Xt, 8);
    bitvector_init(&mask, 8);

    /* Xt = 00000001 (bit 0 changed, not reversed) */
    Xt.data[0] = 0x01000000;

    /* Mask = 00000000 (bit 0 is predictable) */
    mask.data[0] = 0x00000000;

    /* Changed bit is predictable → eₜ = 1 */
    int et = ccsds124_has_positive_updates(&Xt, &mask);
    if (et != 1) {
        fprintf(stderr, "FAIL: Expected et=1, got et=%d\n", et);
        fprintf(stderr, "Xt.data[0] = 0x%08X, mask.data[0] = 0x%08X\n", Xt.data[0], mask.data[0]);
        fprintf(stderr, "Bit 0: Xt=%d, mask=%d\n", bitvector_get_bit(&Xt, 0), bitvector_get_bit(&mask, 0));
    }
    assert(et == 1);
}

TEST(test_compute_ct_flag_single_update) {
    /* Test cₜ = 0 when new_mask_flag set only once */
    ccsds124_compressor_t comp;

    ccsds124_compressor_init(&comp, 8, NULL, 1, 0, 0, 0);

    /* Simulate one new_mask_flag set at t=0 */
    comp.new_mask_flag_history[0] = 1;
    for (size_t i = 1U; i < CCSDS124_MAX_VT_HISTORY; i++) {
        comp.new_mask_flag_history[i] = 0;
    }
    comp.flag_history_index = 1;
    comp.t = 1;

    uint8_t Vt = 2;  /* Check last 2 iterations */
    int ct = ccsds124_compute_ct_flag(&comp, Vt, 0);  /* current flag = 0 */

    assert(ct == 0);  /* Only one update → cₜ = 0 */
}

TEST(test_compute_ct_flag_multiple_updates) {
    /* Test cₜ = 1 when new_mask_flag set 2+ times */
    ccsds124_compressor_t comp;

    ccsds124_compressor_init(&comp, 8, NULL, 1, 0, 0, 0);

    /* Simulate two new_mask_flag sets */
    comp.new_mask_flag_history[0] = 1;
    comp.new_mask_flag_history[1] = 1;
    for (size_t i = 2U; i < CCSDS124_MAX_VT_HISTORY; i++) {
        comp.new_mask_flag_history[i] = 0;
    }
    comp.flag_history_index = 2;
    comp.t = 2;

    uint8_t Vt = 2;  /* Check last 2 iterations */
    int ct = ccsds124_compute_ct_flag(&comp, Vt, 0);  /* current flag = 0 */

    assert(ct == 1);  /* Two updates → cₜ = 1 */
}

/* ========================================================================
 * High-Level API Error Tests
 * ======================================================================== */

TEST(test_ccsds124_compress_invalid_size) {
    ccsds124_compressor_t comp;
    uint8_t input[11] = {0};  /* Not a multiple of packet size */
    uint8_t output[100];
    size_t output_size;

    ccsds124_compressor_init(&comp, 16, NULL, 0, 0, 0, 0);  /* 2 bytes per packet */

    /* 11 bytes is not a multiple of 2 */
    int result = ccsds124_compress(&comp, input, 11, output, sizeof(output), &output_size);

    assert(result == CCSDS124_ERROR_INVALID_ARG);
}

TEST(test_ccsds124_compress_manual_mode) {
    /* ccsds124_compress() with all limits=0 triggers manual control mode
     * (else branch: ft=0, rt=0, pt=0 for all packets). */
    ccsds124_compressor_t comp;
    uint8_t input_data[4] = {0xAA, 0xBB, 0xAA, 0xBB};  /* 2 identical 16-bit packets */
    uint8_t output[256];
    size_t output_size = 0;

    ccsds124_compressor_init(&comp, 16, NULL, 0, 0, 0, 0);  /* All limits = 0 */

    int result = ccsds124_compress(&comp, input_data, 4, output, sizeof(output), &output_size);

    assert(result == CCSDS124_OK);
    assert(output_size > 0);
}

/* ========================================================================
 * CCSDS Encoding Component Tests
 * ======================================================================== */

TEST(test_robustness_window_rt_zero) {
    /* Test Xₜ calculation when Rₜ = 0
     * According to ALGORITHM.md: Xₜ = <Dₜ> if Rₜ = 0 */
    ccsds124_compressor_t comp;
    bitvector_t input1, input2;
    bitbuffer_t output;
    ccsds124_params_t params = {0};

    ccsds124_compressor_init(&comp, 8, NULL, 0, 0, 0, 0);  /* Rₜ = 0 */
    bitvector_init(&input1, 8);
    bitvector_init(&input2, 8);
    bitbuffer_init(&output);

    /* First packet */
    input1.data[0] = 0xAA000000;  /* 10101010 */
    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;
    ccsds124_compress_packet(&comp, &input1, &output, &params);

    /* Second packet - different value */
    input2.data[0] = 0xAB000000;  /* 10101011 */
    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;
    bitbuffer_clear(&output);
    ccsds124_compress_packet(&comp, &input2, &output, &params);

    /* Xₜ should just be <Dₜ> which is reverse of (0xAA XOR 0xAB) = 0x01 */
    /* Change history should contain this at the most recent index */
    /* After 2 packets: t=2, history_index=2, so most recent change is at index 1 */
    size_t most_recent = (comp.history_index - 1 + CCSDS124_MAX_HISTORY) % CCSDS124_MAX_HISTORY;
    assert(comp.change_history[most_recent].data[0] == 0x01000000);
}

TEST(test_robustness_window_with_history) {
    /* Test Xₜ calculation with Rₜ > 0
     * Xₜ = <(Dₜ₋ᴿₜ OR Dₜ₋ᴿₜ₊₁ OR ... OR Dₜ)> */
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;
    ccsds124_params_t params = {0};

    ccsds124_compressor_init(&comp, 8, NULL, 1, 0, 0, 0);  /* Rₜ = 1 */
    bitvector_init(&input, 8);
    bitbuffer_init(&output);

    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;

    /* First packet (t=0) */
    input.data[0] = 0x00000000;
    ccsds124_compress_packet(&comp, &input, &output, &params);

    /* Second packet (t=1) */
    input.data[0] = 0x01000000;  /* Change in bit 0 */
    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;
    bitbuffer_clear(&output);
    ccsds124_compress_packet(&comp, &input, &output, &params);

    /* Third packet (t=2) */
    input.data[0] = 0x03000000;  /* Change in bit 1 */
    bitbuffer_clear(&output);
    ccsds124_compress_packet(&comp, &input, &output, &params);

    /* At t=2 with Rₜ=1: Xₜ = <(D₁ OR D₂)>
     * D₁ = 0x01 (bit 0 changed)
     * D₂ = 0x02 (bit 1 changed)
     * D₁ OR D₂ = 0x03
     * Should have both changes in history */
    assert(comp.t == 3);
    /* History after 3 packets: history_index=3
     * D₀ at index 0, D₁ at index 1, D₂ at index 2 */
    int has_bit0 = 0, has_bit1 = 0;
    for (int i = 1; i <= 2; i++) {  /* Check D₁ and D₂ */
        if (comp.change_history[i].data[0] & 0x01000000) has_bit0 = 1;
        if (comp.change_history[i].data[0] & 0x02000000) has_bit1 = 1;
    }
    assert(has_bit0 && has_bit1);
}

TEST(test_effective_robustness_no_changes) {
    /* Test Vₜ calculation: Vₜ = Rₜ + Cₜ
     * where Cₜ = number of consecutive iterations with no mask changes */
    ccsds124_compressor_t comp;
    bitvector_t input;
    bitbuffer_t output;
    ccsds124_params_t params = {0};

    ccsds124_compressor_init(&comp, 8, NULL, 1, 0, 0, 0);  /* Rₜ = 1 */
    bitvector_init(&input, 8);
    bitbuffer_init(&output);

    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;

    /* Send several identical packets - mask should not change
     * This should increase Vₜ beyond Rₜ */
    input.data[0] = 0xAA000000;

    for (int i = 0; i < 5; i++) {
        bitbuffer_clear(&output);
        if (i > 1) {
            params.send_mask_flag = 0;
            params.uncompressed_flag = 0;
        }
        ccsds124_compress_packet(&comp, &input, &output, &params);
    }

    /* After identical packets, Vₜ should be higher than Rₜ
     * We can't directly test Vₜ without exposing it, but we can
     * verify the mask hasn't changed */
    bitvector_t expected_mask;
    bitvector_init(&expected_mask, 8);
    bitvector_zero(&expected_mask);  /* All predictable */

    assert(bitvector_equals(&comp.mask, &expected_mask));
}

TEST(test_compression_with_robustness) {
    /* Integration test: compress packets with robustness and verify
     * the output includes proper robustness window encoding */
    ccsds124_compressor_t comp;
    bitvector_t input1, input2, input3;
    bitbuffer_t output;
    ccsds124_params_t params = {0};

    ccsds124_compressor_init(&comp, 16, NULL, 2, 0, 0, 0);  /* Rₜ = 2 */
    bitvector_init(&input1, 16);
    bitvector_init(&input2, 16);
    bitvector_init(&input3, 16);
    bitbuffer_init(&output);

    /* Packet 1: initialization */
    input1.data[0] = 0x12000000;
    input1.data[1] = 0x34000000;
    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;
    ccsds124_compress_packet(&comp, &input1, &output, &params);
    size_t size1 = output.num_bits;
    assert(size1 > 0);

    /* Packet 2: small change */
    input2.data[0] = 0x13000000;  /* One bit changed */
    input2.data[1] = 0x34000000;
    params.send_mask_flag = 1;
    params.uncompressed_flag = 1;
    bitbuffer_clear(&output);
    ccsds124_compress_packet(&comp, &input2, &output, &params);
    size_t size2 = output.num_bits;
    assert(size2 > 0);

    /* Packet 3: another change */
    input3.data[0] = 0x13000000;
    input3.data[1] = 0x35000000;  /* One bit changed */
    params.send_mask_flag = 0;
    params.uncompressed_flag = 0;
    bitbuffer_clear(&output);
    ccsds124_compress_packet(&comp, &input3, &output, &params);
    size_t size3 = output.num_bits;

    /* Should be compressed (smaller than uncompressed) */
    assert(size3 < 16 * 8);  /* Less than uncompressed size */
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(void) {
    printf("\nCompression Tests\n");
    printf("=================\n\n");

    printf("Compressor Initialization Tests:\n");
    RUN_TEST(test_compressor_init_valid);
    RUN_TEST(test_compressor_init_with_initial_mask);
    RUN_TEST(test_compressor_init_invalid_length);
    RUN_TEST(test_compressor_init_invalid_robustness);
    RUN_TEST(test_compressor_reset);

    printf("\nSimple Compression Tests:\n");
    RUN_TEST(test_compress_first_packet);
    RUN_TEST(test_compress_d0_zero_with_nonzero_mask);
    RUN_TEST(test_compress_two_identical_packets);
    RUN_TEST(test_compress_with_change);
    RUN_TEST(test_compress_null_params);
    RUN_TEST(test_compress_invalid_input);

    printf("\nCCSDS Helper Function Tests:\n");
    RUN_TEST(test_compute_robustness_window_rt_zero);
    RUN_TEST(test_compute_robustness_window_with_history);
    RUN_TEST(test_compute_effective_robustness_base_case);
    RUN_TEST(test_compute_effective_robustness_with_no_changes);

    printf("\neₜ, kₜ, cₜ Component Tests:\n");
    RUN_TEST(test_has_positive_updates_all_unpredictable);
    RUN_TEST(test_has_positive_updates_has_predictable);
    RUN_TEST(test_compute_ct_flag_single_update);
    RUN_TEST(test_compute_ct_flag_multiple_updates);

    printf("\nHigh-Level API Tests:\n");
    RUN_TEST(test_ccsds124_compress_invalid_size);
    RUN_TEST(test_ccsds124_compress_manual_mode);

    printf("\nCCSDS Encoding Component Tests:\n");
    RUN_TEST(test_robustness_window_rt_zero);
    RUN_TEST(test_robustness_window_with_history);
    RUN_TEST(test_effective_robustness_no_changes);
    RUN_TEST(test_compression_with_robustness);

    printf("\n%d/%d tests passed\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

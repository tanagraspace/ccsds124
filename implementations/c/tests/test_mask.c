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
 * POCKET+ C Implementation - Mask Update Logic Tests
 * TDD: Write tests first, then implement
 *
 * Tests for CCSDS 124.0-B-1 Section 4:
 * - Update build vector (Equation 6)
 * - Update mask vector (Equation 7)
 * - Compute change vector (Equation 8)
 *
 * Authors:
 *   Georges Labrèche <georges@tanagraspace.com> — https://georges.fyi
 *   Claude Code (claude-sonnet-4-5-20250929) <noreply@anthropic.com>
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

/* Forward declarations for mask functions */
void pocket_update_build(
    bitvector_t *build,
    const bitvector_t *input,
    const bitvector_t *prev_input,
    int new_mask_flag,
    size_t t
);

void pocket_update_mask(
    bitvector_t *mask,
    const bitvector_t *input,
    const bitvector_t *prev_input,
    const bitvector_t *build_prev,
    int new_mask_flag
);

void pocket_compute_change(
    bitvector_t *change,
    const bitvector_t *mask,
    const bitvector_t *prev_mask,
    size_t t
);

/* ========================================================================
 * Update Build Vector Tests (CCSDS Equation 6)
 *
 * Bₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁  (if t > 0 and ṗₜ = 0)
 * Bₜ = 0                       (otherwise: t=0 or ṗₜ=1)
 * ======================================================================== */

TEST(test_update_build_at_t0) {
    /* At t=0, Bₜ should always be 0 regardless of inputs */
    bitvector_t build, input, prev_input;

    bitvector_init(&build, 8);
    bitvector_init(&input, 8);
    bitvector_init(&prev_input, 8);

    /* Set some initial values */
    build.data[0] = 0xFF000000;      /* All ones */
    input.data[0] = 0xAB000000;
    prev_input.data[0] = 0xCD000000;

    pocket_update_build(&build, &input, &prev_input, 0, 0);

    /* At t=0, build should be cleared to 0 */
    assert(build.data[0] == 0x00000000);
}

TEST(test_update_build_with_new_mask_flag) {
    /* When ṗₜ = 1 (new_mask_flag set), Bₜ should be 0 */
    bitvector_t build, input, prev_input;

    bitvector_init(&build, 8);
    bitvector_init(&input, 8);
    bitvector_init(&prev_input, 8);

    build.data[0] = 0xFF000000;      /* Current build has data */
    input.data[0] = 0xAB000000;
    prev_input.data[0] = 0xCD000000;

    pocket_update_build(&build, &input, &prev_input, 1, 5);

    /* With new_mask_flag=1, build should be cleared */
    assert(build.data[0] == 0x00000000);
}

TEST(test_update_build_normal_case) {
    /* Normal case: t > 0 and ṗₜ = 0
     * Bₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁
     */
    bitvector_t build, input, prev_input;

    bitvector_init(&build, 8);
    bitvector_init(&input, 8);
    bitvector_init(&prev_input, 8);

    /* Example:
     * Iₜ       = 10110011 = 0xB3
     * Iₜ₋₁     = 10100001 = 0xA1
     * Bₜ₋₁     = 00001100 = 0x0C
     *
     * Iₜ XOR Iₜ₋₁ = 00010010 = 0x12
     * (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ = 00010010 OR 00001100 = 00011110 = 0x1E
     */
    input.data[0] = 0xB3000000;
    prev_input.data[0] = 0xA1000000;
    build.data[0] = 0x0C000000;

    pocket_update_build(&build, &input, &prev_input, 0, 1);

    assert(build.data[0] == 0x1E000000);
}

TEST(test_update_build_no_change) {
    /* If Iₜ = Iₜ₋₁, then XOR yields 0, so Bₜ = Bₜ₋₁ */
    bitvector_t build, input, prev_input;

    bitvector_init(&build, 8);
    bitvector_init(&input, 8);
    bitvector_init(&prev_input, 8);

    input.data[0] = 0xAA000000;
    prev_input.data[0] = 0xAA000000;  /* Same as input */
    build.data[0] = 0x55000000;

    pocket_update_build(&build, &input, &prev_input, 0, 3);

    /* XOR of identical values is 0, so build stays 0x55 */
    assert(build.data[0] == 0x55000000);
}

/* ========================================================================
 * Update Mask Vector Tests (CCSDS Equation 7)
 *
 * Mₜ = (Iₜ XOR Iₜ₋₁) OR Mₜ₋₁    (if ṗₜ = 0)
 * Mₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁    (if ṗₜ = 1)
 * ======================================================================== */

TEST(test_update_mask_normal_case) {
    /* Normal case: ṗₜ = 0
     * Mₜ = (Iₜ XOR Iₜ₋₁) OR Mₜ₋₁
     */
    bitvector_t mask, input, prev_input, build_prev;

    bitvector_init(&mask, 8);
    bitvector_init(&input, 8);
    bitvector_init(&prev_input, 8);
    bitvector_init(&build_prev, 8);

    /* Example:
     * Iₜ       = 11110000 = 0xF0
     * Iₜ₋₁     = 10100000 = 0xA0
     * Mₜ₋₁     = 01000001 = 0x41
     *
     * Iₜ XOR Iₜ₋₁ = 01010000 = 0x50
     * (Iₜ XOR Iₜ₋₁) OR Mₜ₋₁ = 01010000 OR 01000001 = 01010001 = 0x51
     */
    input.data[0] = 0xF0000000;
    prev_input.data[0] = 0xA0000000;
    mask.data[0] = 0x41000000;
    build_prev.data[0] = 0xFF000000;  /* Not used when new_mask_flag=0 */

    pocket_update_mask(&mask, &input, &prev_input, &build_prev, 0);

    assert(mask.data[0] == 0x51000000);
}

TEST(test_update_mask_with_new_mask_flag) {
    /* When ṗₜ = 1 (new_mask_flag set):
     * Mₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁
     */
    bitvector_t mask, input, prev_input, build_prev;

    bitvector_init(&mask, 8);
    bitvector_init(&input, 8);
    bitvector_init(&prev_input, 8);
    bitvector_init(&build_prev, 8);

    /* Example:
     * Iₜ       = 11110000 = 0xF0
     * Iₜ₋₁     = 10100000 = 0xA0
     * Mₜ₋₁     = 01000001 = 0x41 (not used)
     * Bₜ₋₁     = 00001111 = 0x0F
     *
     * Iₜ XOR Iₜ₋₁ = 01010000 = 0x50
     * (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ = 01010000 OR 00001111 = 01011111 = 0x5F
     */
    input.data[0] = 0xF0000000;
    prev_input.data[0] = 0xA0000000;
    mask.data[0] = 0x41000000;       /* Old mask, replaced */
    build_prev.data[0] = 0x0F000000;

    pocket_update_mask(&mask, &input, &prev_input, &build_prev, 1);

    assert(mask.data[0] == 0x5F000000);
}

TEST(test_update_mask_no_change) {
    /* If Iₜ = Iₜ₋₁, XOR yields 0 */
    bitvector_t mask, input, prev_input, build_prev;

    bitvector_init(&mask, 8);
    bitvector_init(&input, 8);
    bitvector_init(&prev_input, 8);
    bitvector_init(&build_prev, 8);

    input.data[0] = 0xCC000000;
    prev_input.data[0] = 0xCC000000;  /* Same */
    mask.data[0] = 0x33000000;
    build_prev.data[0] = 0xFF000000;

    pocket_update_mask(&mask, &input, &prev_input, &build_prev, 0);

    /* XOR is 0, so mask OR 0 = mask = 0x33 */
    assert(mask.data[0] == 0x33000000);
}

/* ========================================================================
 * Compute Change Vector Tests (CCSDS Equation 8)
 *
 * Dₜ = Mₜ XOR Mₜ₋₁  (if t > 0)
 * Dₜ = Mₜ           (if t = 0, assuming M₋₁ = 0)
 * ======================================================================== */

TEST(test_compute_change_at_t0) {
    /* At t=0, D₀ = M₀ XOR M₋₁. In the compression flow, prev_mask is set
     * to mask before calling, so D₀ = M₀ XOR M₀ = 0 (no change at init). */
    bitvector_t change, mask, prev_mask;

    bitvector_init(&change, 8);
    bitvector_init(&mask, 8);
    bitvector_init(&prev_mask, 8);

    mask.data[0] = 0xFF000000;
    prev_mask.data[0] = 0xFF000000;  /* Same as mask (mimics compress.c line 359) */
    change.data[0] = 0xBB000000;  /* Some initial value */

    pocket_compute_change(&change, &mask, &prev_mask, 0);

    /* D₀ = M₀ XOR M₀ = 0 */
    assert(change.data[0] == 0x00000000);
}

TEST(test_compute_change_normal_case) {
    /* Normal case: t > 0
     * Dₜ = Mₜ XOR Mₜ₋₁
     */
    bitvector_t change, mask, prev_mask;

    bitvector_init(&change, 8);
    bitvector_init(&mask, 8);
    bitvector_init(&prev_mask, 8);

    /* Example:
     * Mₜ   = 11001100 = 0xCC
     * Mₜ₋₁ = 10101010 = 0xAA
     *
     * Mₜ XOR Mₜ₋₁ = 01100110 = 0x66
     */
    mask.data[0] = 0xCC000000;
    prev_mask.data[0] = 0xAA000000;

    pocket_compute_change(&change, &mask, &prev_mask, 1);

    assert(change.data[0] == 0x66000000);
}

TEST(test_compute_change_no_mask_change) {
    /* If Mₜ = Mₜ₋₁, then Dₜ = 0 */
    bitvector_t change, mask, prev_mask;

    bitvector_init(&change, 8);
    bitvector_init(&mask, 8);
    bitvector_init(&prev_mask, 8);

    mask.data[0] = 0x77000000;
    prev_mask.data[0] = 0x77000000;  /* Same as current */
    change.data[0] = 0xFF000000;     /* Some initial value */

    pocket_compute_change(&change, &mask, &prev_mask, 5);

    /* XOR of identical values is 0 */
    assert(change.data[0] == 0x00000000);
}

TEST(test_compute_change_all_bits_flip) {
    /* All bits change */
    bitvector_t change, mask, prev_mask;

    bitvector_init(&change, 8);
    bitvector_init(&mask, 8);
    bitvector_init(&prev_mask, 8);

    mask.data[0] = 0xFF000000;
    prev_mask.data[0] = 0x00000000;

    pocket_compute_change(&change, &mask, &prev_mask, 2);

    /* All bits flipped, so XOR yields all 1s */
    assert(change.data[0] == 0xFF000000);
}

/* ========================================================================
 * Integration Tests - Multiple Updates in Sequence
 * ======================================================================== */

TEST(test_mask_update_sequence) {
    /* Test a sequence of updates to ensure state transitions work correctly */
    bitvector_t build, mask, change;
    bitvector_t input_t0, input_t1, input_t2;
    bitvector_t prev_mask;

    bitvector_init(&build, 8);
    bitvector_init(&mask, 8);
    bitvector_init(&change, 8);
    bitvector_init(&prev_mask, 8);
    bitvector_init(&input_t0, 8);
    bitvector_init(&input_t1, 8);
    bitvector_init(&input_t2, 8);

    /* Initial state: all zeros */
    bitvector_zero(&build);
    bitvector_zero(&mask);
    bitvector_zero(&prev_mask);

    /* t=0: First input */
    input_t0.data[0] = 0xAA000000;

    pocket_update_build(&build, &input_t0, NULL, 0, 0);
    assert(build.data[0] == 0x00000000);  /* At t=0, build is 0 */

    pocket_compute_change(&change, &mask, &prev_mask, 0);
    assert(change.data[0] == 0x00000000);  /* At t=0, change is 0 */

    /* t=1: Second input */
    input_t1.data[0] = 0xCC000000;
    bitvector_copy(&prev_mask, &mask);  /* Save previous mask */

    pocket_update_build(&build, &input_t1, &input_t0, 0, 1);
    /* Iₜ XOR Iₜ₋₁ = 0xCC XOR 0xAA = 0x66
     * Bₜ = 0x66 OR 0x00 = 0x66 */
    assert(build.data[0] == 0x66000000);

    pocket_update_mask(&mask, &input_t1, &input_t0, &build, 0);
    /* Mₜ = 0x66 OR 0x00 = 0x66 */
    assert(mask.data[0] == 0x66000000);

    pocket_compute_change(&change, &mask, &prev_mask, 1);
    /* Dₜ = 0x66 XOR 0x00 = 0x66 */
    assert(change.data[0] == 0x66000000);

    /* t=2: Third input, same as previous */
    input_t2.data[0] = 0xCC000000;
    bitvector_copy(&prev_mask, &mask);

    pocket_update_build(&build, &input_t2, &input_t1, 0, 2);
    /* Iₜ XOR Iₜ₋₁ = 0xCC XOR 0xCC = 0x00
     * Bₜ = 0x00 OR 0x66 = 0x66 */
    assert(build.data[0] == 0x66000000);

    pocket_update_mask(&mask, &input_t2, &input_t1, &build, 0);
    /* Mₜ = 0x00 OR 0x66 = 0x66 */
    assert(mask.data[0] == 0x66000000);

    pocket_compute_change(&change, &mask, &prev_mask, 2);
    /* Dₜ = 0x66 XOR 0x66 = 0x00 (no change) */
    assert(change.data[0] == 0x00000000);
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(void) {
    printf("\nMask Update Logic Tests\n");
    printf("========================\n\n");

    printf("Update Build Vector Tests (Equation 6):\n");
    RUN_TEST(test_update_build_at_t0);
    RUN_TEST(test_update_build_with_new_mask_flag);
    RUN_TEST(test_update_build_normal_case);
    RUN_TEST(test_update_build_no_change);

    printf("\nUpdate Mask Vector Tests (Equation 7):\n");
    RUN_TEST(test_update_mask_normal_case);
    RUN_TEST(test_update_mask_with_new_mask_flag);
    RUN_TEST(test_update_mask_no_change);

    printf("\nCompute Change Vector Tests (Equation 8):\n");
    RUN_TEST(test_compute_change_at_t0);
    RUN_TEST(test_compute_change_normal_case);
    RUN_TEST(test_compute_change_no_mask_change);
    RUN_TEST(test_compute_change_all_bits_flip);

    printf("\nIntegration Tests:\n");
    RUN_TEST(test_mask_update_sequence);

    printf("\n%d/%d tests passed\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

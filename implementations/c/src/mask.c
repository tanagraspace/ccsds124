/**
 * @file mask.c
 * @brief POCKET+ mask update logic.
 *
 * @cond INTERNAL
 * ============================================================================
 *  _____                                   ____
 * |_   _|_ _ _ __   __ _  __ _ _ __ __ _  / ___| _ __   __ _  ___ ___
 *   | |/ _` | '_ \ / _` |/ _` | '__/ _` | \___ \| '_ \ / _` |/ __/ _ \
 *   | | (_| | | | | (_| | (_| | | | (_| |  ___) | |_) | (_| | (_|  __/
 *   |_|\__,_|_| |_|\__,_|\__, |_|  \__,_| |____/| .__/ \__,_|\___\___|
 *                        |___/                  |_|
 * ============================================================================
 * @endcond
 *
 * Implements CCSDS 124.0-B-1 Section 4 (Mask Update):
 * - Build Vector Update (Equation 6)
 * - Mask Vector Update (Equation 7)
 * - Change Vector Computation (Equation 8)
 *
 * @authors Georges Labrèche <georges@tanagraspace.com> — https://georges.fyi
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#include "pocketplus.h"

/**
 * @name Build Vector Functions
 *
 * CCSDS Equation 6:
 * - Bₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ (if t > 0 and ṗₜ = 0)
 * - Bₜ = 0 (otherwise: t=0 or ṗₜ=1)
 *
 * The build vector accumulates bits that have changed over time.
 * When new_mask_flag is set, the build is used to replace the mask
 * and is then reset to zero.
 * @{
 */


void pocket_update_build(
    bitvector_t *build,
    const bitvector_t *input,
    const bitvector_t *prev_input,
    int new_mask_flag,
    size_t t
) {
    /* Case 1: t=0 or new_mask_flag set → reset build to 0 */
    if ((t == 0U) || (new_mask_flag != 0)) {
        bitvector_zero(build);
    } else {
        /* Case 2: Normal operation (t > 0 and new_mask_flag = 0)
         * Bₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁
         */

        /* Create temporary vector for changes */
        bitvector_t changes;
        (void)bitvector_init(&changes, build->length);

        /* Calculate changes: Iₜ XOR Iₜ₋₁ */
        bitvector_xor(&changes, input, prev_input);

        /* Update build: Bₜ = changes OR Bₜ₋₁ */
        bitvector_or(build, &changes, build);
    }
}

/** @} */ /* End of Build Vector Functions */

/**
 * @name Mask Vector Functions
 *
 * CCSDS Equation 7:
 * - Mₜ = (Iₜ XOR Iₜ₋₁) OR Mₜ₋₁ (if ṗₜ = 0)
 * - Mₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ (if ṗₜ = 1)
 *
 * The mask tracks which bits are unpredictable.
 * When new_mask_flag is set, the mask is replaced with the build vector.
 * @{
 */


void pocket_update_mask(
    bitvector_t *mask,
    const bitvector_t *input,
    const bitvector_t *prev_input,
    const bitvector_t *build_prev,
    int new_mask_flag
) {
    /* Create temporary vector for changes */
    bitvector_t changes;
    (void)bitvector_init(&changes, mask->length);

    /* Calculate changes: Iₜ XOR Iₜ₋₁ */
    bitvector_xor(&changes, input, prev_input);

    if (new_mask_flag != 0) {
        /* Case 1: new_mask_flag set → Mₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ */
        bitvector_or(mask, &changes, build_prev);
    } else {
        /* Case 2: Normal operation → Mₜ = (Iₜ XOR Iₜ₋₁) OR Mₜ₋₁ */
        bitvector_or(mask, &changes, mask);
    }
}

/** @} */ /* End of Mask Vector Functions */

/**
 * @name Change Vector Functions
 *
 * CCSDS Equation 8:
 * - Dₜ = Mₜ XOR Mₜ₋₁
 *
 * The change vector tracks which mask bits changed between time steps.
 * This is used in encoding to communicate mask updates.
 *
 * At t=0, the caller sets Mₜ₋₁ = M₀ so that D₀ = M₀ XOR M₀ = 0
 * (no change at initialization).
 * @{
 */


void pocket_compute_change(
    bitvector_t *change,
    const bitvector_t *mask,
    const bitvector_t *prev_mask,
    size_t t
) {
    (void)t;  /* Always use XOR — caller sets prev_mask appropriately */
    /* Dₜ = Mₜ XOR Mₜ₋₁ */
    bitvector_xor(change, mask, prev_mask);
}

/** @} */ /* End of Change Vector Functions */

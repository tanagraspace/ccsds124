/**
 * @file compress.c
 * @brief CCSDS 124.0-B-1 compression algorithm implementation.
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
 * Implements CCSDS 124.0-B-1 Section 5.3 (Encoding Step):
 * - Compressor initialization and state management
 * - Main compression algorithm
 * - Output packet encoding: oₜ = hₜ ∥ qₜ ∥ uₜ
 *
 * @authors Georges Labrèche <georges@tanagraspace.com> — https://georges.fyi
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#include "ccsds124.h"
#include <string.h>

/**
 * @name Compressor Initialization
 * @{
 */


int ccsds124_compressor_init(
    ccsds124_compressor_t *comp,
    size_t F,
    const bitvector_t *initial_mask,
    uint8_t robustness,
    int pt_limit,
    int ft_limit,
    int rt_limit
) {
    int result = CCSDS124_ERROR_INVALID_ARG;

    if (comp != NULL) {
        if ((F == 0U) || (F > (size_t)CCSDS124_MAX_PACKET_LENGTH)) {
            /* Invalid packet length - result already set */
        } else if (robustness > (uint8_t)CCSDS124_MAX_ROBUSTNESS) {
            /* Invalid robustness - result already set */
        } else {
            /* Store configuration */
            comp->F = F;
            comp->robustness = robustness;
            comp->pt_limit = pt_limit;
            comp->ft_limit = ft_limit;
            comp->rt_limit = rt_limit;

            /* Initialize all bit vectors */
            (void)bitvector_init(&comp->mask, F);
            (void)bitvector_init(&comp->prev_mask, F);
            (void)bitvector_init(&comp->build, F);
            (void)bitvector_init(&comp->prev_input, F);
            (void)bitvector_init(&comp->initial_mask, F);

            /* Initialize change history */
            for (size_t i = 0U; i < CCSDS124_MAX_HISTORY; i++) {
                (void)bitvector_init(&comp->change_history[i], F);
                bitvector_zero(&comp->change_history[i]);
            }

            /* Initialize flag history for cₜ calculation */
            for (size_t i = 0U; i < CCSDS124_MAX_VT_HISTORY; i++) {
                comp->new_mask_flag_history[i] = 0U;
            }
            comp->flag_history_index = 0;

            /* Set initial mask */
            if (initial_mask != NULL) {
                bitvector_copy(&comp->initial_mask, initial_mask);
                bitvector_copy(&comp->mask, initial_mask);
            } else {
                bitvector_zero(&comp->initial_mask);
                bitvector_zero(&comp->mask);
            }

            /* Initialize work buffers (pre-allocated to avoid per-packet init) */
            (void)bitvector_init(&comp->work_prev_build, F);
            (void)bitvector_init(&comp->work_change, F);
            (void)bitvector_init(&comp->work_Xt, F);
            (void)bitvector_init(&comp->work_inverted, F);
            (void)bitvector_init(&comp->work_shifted, F);
            (void)bitvector_init(&comp->work_diff, F);

            /* Reset state */
            ccsds124_compressor_reset(comp);

            result = CCSDS124_OK;
        }
    }

    return result;
}


void ccsds124_compressor_reset(ccsds124_compressor_t *comp) {
    if (comp != NULL) {
        /* Reset time index */
        comp->t = 0U;
        comp->history_index = 0U;

        /* Reset mask to initial */
        bitvector_copy(&comp->mask, &comp->initial_mask);
        bitvector_zero(&comp->prev_mask);

        /* Clear build and prev_input */
        bitvector_zero(&comp->build);
        bitvector_zero(&comp->prev_input);

        /* Clear change history */
        for (size_t i = 0U; i < CCSDS124_MAX_HISTORY; i++) {
            bitvector_zero(&comp->change_history[i]);
        }

        /* Reset countdown counters (match reference implementation behavior) */
        comp->pt_counter = comp->pt_limit;
        comp->ft_counter = comp->ft_limit;
        comp->rt_counter = comp->rt_limit;
    }
}

/** @} */ /* End of Compressor Initialization */

/**
 * @name Internal Helper Functions
 * @{
 */

/**
 * @brief Get default compression parameters.
 *
 * Initializes parameters with robustness from compressor and all flags cleared.
 *
 * @param[out] params Pointer to parameters structure.
 * @param[in]  comp   Pointer to compressor for robustness level.
 *
 * @note Caller should handle CCSDS initialization requirements
 *       (ft=1, rt=1 for first Rt+1 packets).
 */
static void get_default_params(
    ccsds124_params_t *params,
    const ccsds124_compressor_t *comp
) {
    /* Default parameters */
    params->min_robustness = comp->robustness;
    params->new_mask_flag = 0;
    params->send_mask_flag = 0;
    params->uncompressed_flag = 0;

    /* Note: Caller should handle CCSDS initialization requirements
     * (ft=1, rt=1 for first Rt+1 packets) */
}

/** @} */ /* End of Internal Helper Functions */

/**
 * @name CCSDS Helper Functions
 * @{
 */


void ccsds124_compute_robustness_window(
    bitvector_t *Xt,
    const ccsds124_compressor_t *comp,
    const bitvector_t *current_change
) {
    /* Xₜ = <(Dₜ₋ᴿₜ OR Dₜ₋ᴿₜ₊₁ OR ... OR Dₜ)>
     * Where <a> means reverse the bit order */

    /* Use Xt directly as accumulator to avoid stack allocations */
    Xt->length = comp->F;
    Xt->num_words = (((comp->F + 7U) / 8U) + 3U) / 4U;

    if ((comp->robustness == 0U) || (comp->t == 0U)) {
        /* Xₜ = Dₜ (no reversal - RLE processes LSB to MSB directly) */
        bitvector_copy(Xt, current_change);
    } else {
        /* Start with current change, accumulate OR directly into Xt */
        bitvector_copy(Xt, current_change);

        /* Determine how many historical changes to include */
        size_t num_changes = (comp->t < (size_t)comp->robustness) ? comp->t : (size_t)comp->robustness;

        /* OR with historical changes (going backwards from current)
         * Use in-place OR: Xt = Xt OR history[i] */
        for (size_t i = 1U; i <= num_changes; i++) {
            size_t hist_idx = ((comp->history_index + (size_t)CCSDS124_MAX_HISTORY) - i) % (size_t)CCSDS124_MAX_HISTORY;
            const bitvector_t *hist = &comp->change_history[hist_idx];

            /* In-place OR at word level */
            for (size_t w = 0U; w < Xt->num_words; w++) {
                Xt->data[w] |= hist->data[w];
            }
        }
    }
}


uint8_t ccsds124_compute_effective_robustness(
    const ccsds124_compressor_t *comp,
    const bitvector_t *current_change
) {
    /* Vₜ = Rₜ + Cₜ (CCSDS Section 5.3.2.2)
     * where Cₜ = number of consecutive iterations with no mask changes
     *
     * For t ≤ Rt: Vt = Rt
     * For t > Rt: Vt = Rt + Ct */

    (void)current_change;  /* Unused - Ct is computed from history */

    uint8_t Rt = comp->robustness;
    uint8_t Vt = Rt;

    /* For t > Rt, compute Ct */
    if (comp->t > (size_t)Rt) {
        /* Count backwards through history starting from Rt+1 positions back
         * (matching reference: start = pt_history_index + Rt + 1) */
        uint8_t Ct = 0U;
        int done = 0;

        for (size_t i = (size_t)Rt + 1U; (i <= 15U) && (i <= comp->t) && (done == 0); i++) {
            size_t hist_idx = ((comp->history_index + (size_t)CCSDS124_MAX_HISTORY) - i) % (size_t)CCSDS124_MAX_HISTORY;
            if (bitvector_hamming_weight(&comp->change_history[hist_idx]) > 0U) {
                done = 1;  /* Found a change, stop counting */
            } else {
                Ct++;
                if (Ct >= (15U - Rt)) {
                    done = 1;  /* Cap at maximum Ct value */
                }
            }
        }

        Vt = Rt + Ct;
        if (Vt > 15U) {
            Vt = 15U;  /* Cap at 15 (4 bits) */
        }
    }

    return Vt;
}


int ccsds124_has_positive_updates(
    const bitvector_t *Xt,
    const bitvector_t *mask
) {
    int result = 0;  /* No positive updates by default */

    /* eₜ = 1 if any changed bits (in Xₜ) are predictable (mask bit = 0)
     * This is equivalent to: (Xt AND (NOT mask)) != 0
     * Use word-level operations for efficiency */

    if ((Xt != NULL) && (mask != NULL)) {
        size_t num_words = Xt->num_words;
        if (mask->num_words < num_words) {
            num_words = mask->num_words;
        }

        for (size_t i = 0U; (i < num_words) && (result == 0); i++) {
            /* Check if any bit in Xt is set where mask is clear */
            if ((Xt->data[i] & ~mask->data[i]) != 0U) {
                result = 1;
            }
        }
    }

    return result;
}


int ccsds124_compute_ct_flag(
    const ccsds124_compressor_t *comp,
    uint8_t Vt,
    int current_new_mask_flag
) {
    int result = 0;

    /* cₜ = 1 if new_mask_flag was set 2+ times in last Vₜ+1 iterations
     * (including current packet) - matches reference implementation */

    if (Vt != 0U) {
        /* Count how many times new_mask_flag was set.
         * Reference checks from current (i=pt_history_index) to current+Vt,
         * which is Vt+1 entries including current packet */
        int count = 0;

        /* Include current packet's flag */
        if (current_new_mask_flag != 0) {
            count++;
        }

        /* Check history for Vt previous entries */
        size_t iterations_to_check = ((size_t)Vt <= comp->t) ? (size_t)Vt : comp->t;

        for (size_t i = 0U; i < iterations_to_check; i++) {
            /* Calculate history index going backwards from previous */
            size_t hist_idx = ((comp->flag_history_index + (size_t)CCSDS124_MAX_VT_HISTORY) - 1U - i) % (size_t)CCSDS124_MAX_VT_HISTORY;
            if (comp->new_mask_flag_history[hist_idx] != 0U) {
                count++;
            }
        }

        if (count >= 2) {
            result = 1;
        }
    }

    return result;
}

/** @} */ /* End of CCSDS Helper Functions */

/**
 * @name Main Compression Functions
 * @{
 */


int ccsds124_compress_packet(
    ccsds124_compressor_t *comp,
    const bitvector_t *input,
    bitbuffer_t *output,
    const ccsds124_params_t *params
) {
    int result = CCSDS124_ERROR_INVALID_ARG;

    if ((comp == NULL) || (input == NULL) || (output == NULL)) {
        /* Invalid arguments - result already set */
    } else if (input->length != comp->F) {
        /* Invalid input length - result already set */
    } else {
        /* Get parameters (use defaults if NULL) */
        ccsds124_params_t local_params;
        const ccsds124_params_t *effective_params = params;
        if (params == NULL) {
            get_default_params(&local_params, comp);
            effective_params = &local_params;
        }

        /* Clear output buffer */
        bitbuffer_clear(output);

        /* ====================================================================
         * STEP 1: Update Mask and Build Vectors (CCSDS Section 4)
         * ==================================================================== */

        /* Use pre-allocated prev_mask from struct */
        bitvector_copy(&comp->prev_mask, &comp->mask);

        /* Use pre-allocated work buffer for prev_build */
        bitvector_copy(&comp->work_prev_build, &comp->build);

        /* Update build vector (Equation 6) */
        if (comp->t > 0U) {
            ccsds124_update_build(&comp->build, input, &comp->prev_input,
                               effective_params->new_mask_flag, comp->t);
        }

        /* Update mask vector (Equation 7) */
        if (comp->t > 0U) {
            ccsds124_update_mask(&comp->mask, input, &comp->prev_input,
                              &comp->work_prev_build, effective_params->new_mask_flag);
        }

        /* Compute change vector (Equation 8) - use pre-allocated work buffer */
        ccsds124_compute_change(&comp->work_change, &comp->mask, &comp->prev_mask, comp->t);

        /* Store change in history (circular buffer) */
        bitvector_copy(&comp->change_history[comp->history_index], &comp->work_change);

        /* ====================================================================
         * STEP 2: Encode Output Packet (CCSDS Section 5.3)
         * oₜ = hₜ ∥ qₜ ∥ uₜ
         * Full CCSDS encoding per ALGORITHM.md
         * ==================================================================== */

        /* Calculate Xₜ (robustness window) - use pre-allocated work buffer */
        ccsds124_compute_robustness_window(&comp->work_Xt, comp, &comp->work_change);

        /* Calculate Vₜ (effective robustness) */
        uint8_t Vt = ccsds124_compute_effective_robustness(comp, &comp->work_change);

        /* Calculate ḋₜ flag */
        uint8_t dt = ((effective_params->send_mask_flag == 0U) && (effective_params->uncompressed_flag == 0U)) ? 1U : 0U;

        /* ====================================================================
         * Component hₜ: Mask change information
         * hₜ = RLE(Xₜ) ∥ BIT₄(Vₜ) ∥ eₜ ∥ kₜ ∥ cₜ ∥ ḋₜ
         * ==================================================================== */

        /* 1. RLE(Xₜ) - Run-length encode the robustness window */
        (void)ccsds124_rle_encode(output, &comp->work_Xt);

        /* 2. BIT₄(Vₜ) - 4-bit effective robustness level
         * CCSDS encodes Vt directly (reference implementation confirmed) */
        for (int i = 3; i >= 0; i--) {
            uint32_t vt_shifted = (uint32_t)Vt >> (uint32_t)i;
            uint32_t vt_masked = vt_shifted & 1U;
            int bit_val = (int)vt_masked;
            (void)bitbuffer_append_bit(output, bit_val);
        }

        /* 3. eₜ, kₜ, cₜ - Only if Vₜ > 0 and there are mask changes */
        if ((Vt > 0U) && (bitvector_hamming_weight(&comp->work_Xt) > 0U)) {
            /* Calculate eₜ */
            int et = ccsds124_has_positive_updates(&comp->work_Xt, &comp->mask);

            (void)bitbuffer_append_bit(output, et);

            if (et != 0) {
                /* kₜ - Output '1' for positive updates (mask=0), '0' for negative updates (mask=1)
                 * Reference implementation shows kt outputs 1 when mask bit is 0 at changed positions */

                /* Extract INVERTED mask values (1 where mask=0, 0 where mask=1)
                 * Use pre-allocated work buffer for inverted mask */
                bitvector_not(&comp->work_inverted, &comp->mask);

                (void)ccsds124_bit_extract_forward(output, &comp->work_inverted, &comp->work_Xt);

                /* Calculate and encode cₜ */
                int ct = ccsds124_compute_ct_flag(comp, Vt, effective_params->new_mask_flag);

                (void)bitbuffer_append_bit(output, ct);
            }
        }

        /* 4. ḋₜ - Flag indicating if both ḟₜ and ṙₜ are zero */
        (void)bitbuffer_append_bit(output, (int)dt);

        /* ====================================================================
         * Component qₜ: Optional full mask
         * qₜ = ∅ if ḋₜ=1, '1' ∥ RLE(<(Mₜ XOR (Mₜ≪))>) if ḟₜ=1, '0' otherwise
         * ==================================================================== */

        if (dt == 0U) {  /* Only if ḋₜ = 0 */
            if (effective_params->send_mask_flag != 0U) {
                (void)bitbuffer_append_bit(output, 1);  /* Flag: mask follows */

                /* Encode mask as RLE(M XOR (M<<)) - use pre-allocated work buffers */
                bitvector_left_shift(&comp->work_shifted, &comp->mask);
                bitvector_xor(&comp->work_diff, &comp->mask, &comp->work_shifted);

                (void)ccsds124_rle_encode(output, &comp->work_diff);
            } else {
                (void)bitbuffer_append_bit(output, 0);  /* Flag: no mask */
            }
        }

        /* ====================================================================
         * Component uₜ: Unpredictable bits or full input
         * Per ALGORITHM.md, depends on ḋₜ, ṙₜ, cₜ
         * ==================================================================== */

        if (effective_params->uncompressed_flag != 0U) {
            /* '1' ∥ COUNT(F) ∥ Iₜ */
            (void)bitbuffer_append_bit(output, 1);  /* Flag: full input follows */

            (void)ccsds124_count_encode(output, (uint32_t)comp->F);

            (void)bitbuffer_append_bitvector(output, input);
        } else {
            if (dt == 0U) {
                /* '0' ∥ BE(...) */
                (void)bitbuffer_append_bit(output, 0);  /* Flag: compressed */
            }

            /* Determine extraction mask based on cₜ */
            int ct = ccsds124_compute_ct_flag(comp, Vt, effective_params->new_mask_flag);

            if ((ct != 0) && (Vt > 0U)) {
                /* BE(Iₜ, (Xₜ OR Mₜ)) - extract bits where mask OR changes are set
                 * Reuse work_diff as extraction mask (not used at this point) */
                bitvector_or(&comp->work_diff, &comp->mask, &comp->work_Xt);  /* Mₜ OR Xₜ */

                (void)ccsds124_bit_extract(output, input, &comp->work_diff);
            } else {
                /* BE(Iₜ, Mₜ) - extract only unpredictable bits */
                (void)ccsds124_bit_extract(output, input, &comp->mask);
            }
        }

        /* ====================================================================
         * STEP 3: Update State for Next Cycle
         * ==================================================================== */

        /* Save current input and mask as previous for next iteration */
        bitvector_copy(&comp->prev_input, input);
        bitvector_copy(&comp->prev_mask, &comp->mask);

        /* Track new_mask_flag for cₜ calculation */
        comp->new_mask_flag_history[comp->flag_history_index] = effective_params->new_mask_flag;
        comp->flag_history_index = (comp->flag_history_index + 1U) % (size_t)CCSDS124_MAX_VT_HISTORY;

        /* Advance time */
        comp->t++;

        /* Advance history index (circular buffer) */
        comp->history_index = (comp->history_index + 1U) % (size_t)CCSDS124_MAX_HISTORY;

        result = CCSDS124_OK;
    }

    return result;
}


int ccsds124_compress(
    ccsds124_compressor_t *comp,
    const uint8_t *input_data,
    size_t input_size,
    uint8_t *output_buffer,
    size_t output_buffer_size,
    size_t *output_size
) {
    int result = CCSDS124_ERROR_INVALID_ARG;

    if ((comp == NULL) || (input_data == NULL) || (output_buffer == NULL) || (output_size == NULL)) {
        /* Invalid arguments - result already set */
    } else {
        /* Calculate packet size in bytes */
        size_t packet_size_bytes = (comp->F + 7U) / 8U;

        /* Verify input size is a multiple of packet size */
        if ((input_size % packet_size_bytes) != 0U) {
            /* Invalid input size - result already set */
        } else {
            /* Calculate number of packets */
            size_t num_packets = input_size / packet_size_bytes;

            /* Reset compressor state */
            ccsds124_compressor_reset(comp);

            /* Output accumulation */
            size_t total_output_bytes = 0U;
            int compress_error = 0;

            /* Process each packet */
            for (size_t i = 0U; (i < num_packets) && (compress_error == 0); i++) {
                bitvector_t input_vec;
                bitbuffer_t packet_output;

                (void)bitvector_init(&input_vec, comp->F);
                bitbuffer_init(&packet_output);

                /* Load packet data */
                (void)bitvector_from_bytes(&input_vec, &input_data[i * packet_size_bytes], packet_size_bytes);

                /* Compute parameters */
                ccsds124_params_t params;
                params.min_robustness = comp->robustness;

                /* If limits are set, manage parameters automatically using countdown counters
                 * (matches reference implementation exactly) */
                if ((comp->pt_limit > 0) && (comp->ft_limit > 0) && (comp->rt_limit > 0)) {
                    /* Reference implementation: packet 1 handled separately, loop starts at packet 2 (1-indexed).
                     * In 0-indexed: packet 0 = handled separately, counters start at packet 1. */
                    if (i == 0U) {
                        /* First packet: fixed init values, counters not checked */
                        params.send_mask_flag = 1;
                        params.uncompressed_flag = 1;
                        params.new_mask_flag = 0;
                    } else {
                        /* Packets 1+: check and update countdown counters */
                        /* ft counter */
                        if (comp->ft_counter == 1) {
                            params.send_mask_flag = 1;
                            comp->ft_counter = comp->ft_limit;
                        } else {
                            comp->ft_counter--;
                            params.send_mask_flag = 0;
                        }

                        /* pt counter */
                        if (comp->pt_counter == 1) {
                            params.new_mask_flag = 1;
                            comp->pt_counter = comp->pt_limit;
                        } else {
                            comp->pt_counter--;
                            params.new_mask_flag = 0;
                        }

                        /* rt counter */
                        if (comp->rt_counter == 1) {
                            params.uncompressed_flag = 1;
                            comp->rt_counter = comp->rt_limit;
                        } else {
                            comp->rt_counter--;
                            params.uncompressed_flag = 0;
                        }

                        /* Override for remaining init packets: CCSDS requires first Rt+1 packets
                         * to have ft=1, rt=1, pt=0. Reference checks: if (i < Rt+2) in 1-indexed.
                         * In 0-indexed: if (i <= Rt). Counters are still decremented per reference. */
                        if (i <= (size_t)comp->robustness) {
                            params.send_mask_flag = 1;
                            params.uncompressed_flag = 1;
                            params.new_mask_flag = 0;
                        }
                    }
                } else {
                    /* Manual control: use defaults (ft=0, rt=0, pt=0 for normal operation) */
                    params.send_mask_flag = 0;
                    params.uncompressed_flag = 0;
                    params.new_mask_flag = 0;
                }

                /* Compress packet */
                int packet_result = ccsds124_compress_packet(comp, &input_vec, &packet_output, &params);
                if (packet_result != CCSDS124_OK) {
                    result = packet_result;
                    compress_error = 1;
                } else {
                    /* Convert packet to bytes with byte-boundary padding */
                    uint8_t packet_bytes[CCSDS124_MAX_OUTPUT_BYTES];
                    size_t packet_size = bitbuffer_to_bytes(&packet_output, packet_bytes, sizeof(packet_bytes));

                    /* Check if output buffer has space */
                    if ((total_output_bytes + packet_size) > output_buffer_size) {
                        result = CCSDS124_ERROR_OVERFLOW;
                        compress_error = 1;
                    } else {
                        /* Append to output buffer */
                        (void)memcpy(&output_buffer[total_output_bytes], packet_bytes, packet_size);
                        total_output_bytes += packet_size;
                    }
                }
            }

            /* Return total output size if successful */
            if (compress_error == 0) {
                *output_size = total_output_bytes;
                result = CCSDS124_OK;
            }
        }
    }

    return result;
}

/** @} */ /* End of Main Compression Functions */

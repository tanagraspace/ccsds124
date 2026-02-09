/**
 * @file decompress.c
 * @brief POCKET+ decompression algorithm implementation.
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
 * Implements CCSDS 124.0-B-1 decompression (inverse of Section 5.3):
 * - Bit reader for parsing compressed packets
 * - COUNT and RLE decoding
 * - Packet decompression and mask reconstruction
 *
 * Code follows MISRA C:2012 guidelines.
 *
 * @authors Georges Labrèche <georges@tanagraspace.com> — https://georges.fyi
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#include "pocketplus.h"
#include <string.h>

/**
 * @name Internal Helper Functions
 * @{
 */

/**
 * @brief Extract positions of all set bits in a bitvector using word-level processing.
 *
 * Much faster than iterating bit-by-bit when the bitvector is sparse.
 * Uses __builtin_clz for O(1) MSB position finding.
 *
 * @param[in]  bv         Bitvector to scan
 * @param[out] positions  Array to store positions (must be at least hamming_weight elements)
 * @param[in]  max_pos    Maximum positions to extract
 * @return Number of positions extracted
 */
static size_t bitvector_get_set_positions(
    const bitvector_t *bv,
    size_t *positions,
    size_t max_pos
) {
    size_t count = 0U;

    /* Process words in forward order to get positions in ascending order */
    for (size_t word = 0U; (word < bv->num_words) && (count < max_pos); word++) {
        uint32_t word_data = bv->data[word];

        while ((word_data != 0U) && (count < max_pos)) {
            /* Find MSB position using count leading zeros */
            int clz = __builtin_clz(word_data);
            size_t bit_pos_in_word = (size_t)clz;

            /* Global position */
            size_t global_pos = (word * 32U) + bit_pos_in_word;

            if (global_pos < bv->length) {
                positions[count] = global_pos;
                count++;
            }

            /* Clear the MSB we just processed */
            word_data &= ~(1U << (31U - (uint32_t)clz));
        }
    }

    return count;
}

/** @} */ /* End of Internal Helper Functions */

/**
 * @name Bit Reader Functions
 * @{
 */


void bitreader_init(bitreader_t *reader, const uint8_t *data, size_t num_bits) {
    if (reader != NULL) {
        reader->data = data;
        reader->num_bits = num_bits;
        reader->bit_pos = 0U;
    }
}


int bitreader_read_bit(bitreader_t *reader) {
    int result = -1;

    if ((reader != NULL) && (reader->bit_pos < reader->num_bits)) {
        size_t byte_index = reader->bit_pos / 8U;
        size_t bit_index = reader->bit_pos % 8U;

        /* MSB-first: bit 0 of byte is at position 7 */
        uint8_t shifted = reader->data[byte_index] >> (7U - bit_index);
        uint8_t masked = shifted & 1U;
        result = (int)masked;
        reader->bit_pos++;
    }

    return result;
}


uint32_t bitreader_read_bits(bitreader_t *reader, size_t num_bits) {
    uint32_t value = 0U;

    if ((reader != NULL) && (num_bits <= 32U)) {
        /* Check sufficient bits remain before reading */
        if (bitreader_remaining(reader) < num_bits) {
            return 0U;
        }

        for (size_t i = 0U; i < num_bits; i++) {
            int bit = bitreader_read_bit(reader);
            if (bit >= 0) {
                value = (value << 1U) | ((uint32_t)bit & 1U);
            }
        }
    }

    return value;
}


size_t bitreader_position(const bitreader_t *reader) {
    size_t pos = 0U;

    if (reader != NULL) {
        pos = reader->bit_pos;
    }

    return pos;
}


size_t bitreader_remaining(const bitreader_t *reader) {
    size_t remaining = 0U;

    if ((reader != NULL) && (reader->bit_pos < reader->num_bits)) {
        remaining = reader->num_bits - reader->bit_pos;
    }

    return remaining;
}


void bitreader_align_byte(bitreader_t *reader) {
    if (reader != NULL) {
        size_t bit_offset = reader->bit_pos % 8U;
        if (bit_offset != 0U) {
            reader->bit_pos += (8U - bit_offset);
        }
    }
}

/** @} */ /* End of Bit Reader Functions */

/**
 * @name Decoding Functions
 * @{
 */


int pocket_count_decode(bitreader_t *reader, uint32_t *value) {
    int result = POCKET_ERROR_INVALID_ARG;

    if ((reader == NULL) || (value == NULL)) {
        return result;
    }

    if (bitreader_remaining(reader) == 0U) {
        return POCKET_ERROR_UNDERFLOW;
    }

    /* Read first bit */
    int bit0 = bitreader_read_bit(reader);

    if (bit0 == 0) {
        /* Case 1: '0' → value is 1 */
        *value = 1U;
        result = POCKET_OK;
    } else {
        /* First bit is 1, read second bit */
        int bit1 = bitreader_read_bit(reader);

        if (bit1 == 0) {
            /* Case 2: '10' → terminator (value 0) */
            *value = 0U;
            result = POCKET_OK;
        } else {
            /* First two bits are 11, read third bit */
            int bit2 = bitreader_read_bit(reader);

            if (bit2 == 0) {
                /* Case 3: '110' + 5 bits → value + 2 */
                if (bitreader_remaining(reader) < 5U) {
                    return POCKET_ERROR_UNDERFLOW;
                }
                uint32_t raw = bitreader_read_bits(reader, 5U);
                *value = raw + 2U;
                result = POCKET_OK;
            } else {
                /* Case 4: '111' + variable bits
                 * Need to find the size by counting zeros until a 1 */
                size_t size = 0U;
                int next_bit = 0;

                /* Count zeros to determine field size */
                do {
                    next_bit = bitreader_read_bit(reader);
                    if (next_bit < 0) {
                        return POCKET_ERROR_UNDERFLOW;
                    }
                    size++;
                } while (next_bit == 0);

                /* Size of value field is size + 5 */
                size_t value_bits = size + 5U;

                /* Back up one bit since the '1' is part of the value */
                reader->bit_pos--;

                /* Check sufficient bits remain for value field */
                if (bitreader_remaining(reader) < value_bits) {
                    return POCKET_ERROR_UNDERFLOW;
                }

                /* Read the value field */
                uint32_t raw = bitreader_read_bits(reader, value_bits);
                *value = raw + 2U;
                result = POCKET_OK;
            }
        }
    }

    return result;
}


int pocket_rle_decode(bitreader_t *reader, bitvector_t *result, size_t length) {
    int status = POCKET_ERROR_INVALID_ARG;

    if ((reader == NULL) || (result == NULL)) {
        return status;
    }

    /* Initialize result to all zeros */
    (void)bitvector_init(result, length);
    bitvector_zero(result);

    /* Start from end of vector (matching RLE encoding which processes LSB to MSB) */
    size_t bit_position = length;

    /* Read COUNT values until terminator */
    uint32_t delta = 0U;
    status = pocket_count_decode(reader, &delta);

    while ((status == POCKET_OK) && (delta != 0U)) {
        /* Delta represents (count of zeros + 1) */
        if (delta > bit_position) {
            /* Invalid: delta exceeds remaining positions (v1.6/v1.7/v1.8) */
            return POCKET_ERROR_OVERFLOW;
        }
        bit_position -= delta;
        /* Set the bit at this position */
        bitvector_set_bit(result, bit_position, 1);

        /* Read next delta */
        status = pocket_count_decode(reader, &delta);
    }

    return status;
}


int pocket_bit_insert(bitreader_t *reader, bitvector_t *data, const bitvector_t *mask) {
    int status = POCKET_ERROR_INVALID_ARG;

    if ((reader == NULL) || (data == NULL) || (mask == NULL)) {
        return status;
    }

    if (data->length != mask->length) {
        return status;
    }

    size_t hamming = bitvector_hamming_weight(mask);

    if (hamming == 0U) {
        /* No bits to insert */
        return POCKET_OK;
    }

    /* Check sufficient bits remain before reading */
    if (bitreader_remaining(reader) < hamming) {
        return POCKET_ERROR_UNDERFLOW;
    }

    /* Collect positions of '1' bits in mask using word-level processing */
    size_t positions[POCKET_MAX_PACKET_LENGTH];
    size_t pos_count = bitvector_get_set_positions(mask, positions, hamming);

    /* Insert bits in reverse order (matching BE extraction) */
    for (size_t i = pos_count; i > 0U; i--) {
        int bit = bitreader_read_bit(reader);
        if (bit < 0) {
            return POCKET_ERROR_UNDERFLOW;
        }
        bitvector_set_bit(data, positions[i - 1U], bit);
    }

    status = POCKET_OK;
    return status;
}

/** @} */ /* End of Decoding Functions */

/**
 * @name Decompressor Initialization
 * @{
 */


int pocket_decompressor_init(
    pocket_decompressor_t *decomp,
    size_t F,
    const bitvector_t *initial_mask,
    uint8_t robustness
) {
    if (decomp == NULL) {
        return POCKET_ERROR_INVALID_ARG;
    }

    if ((F == 0U) || (F > (size_t)POCKET_MAX_PACKET_LENGTH)) {
        return POCKET_ERROR_INVALID_ARG;
    }

    if (robustness > (uint8_t)POCKET_MAX_ROBUSTNESS) {
        return POCKET_ERROR_INVALID_ARG;
    }

    /* Store configuration */
    decomp->F = F;
    decomp->robustness = robustness;

    /* Initialize bit vectors */
    (void)bitvector_init(&decomp->mask, F);
    (void)bitvector_init(&decomp->initial_mask, F);
    (void)bitvector_init(&decomp->prev_output, F);
    (void)bitvector_init(&decomp->Xt, F);

    /* Set initial mask */
    if (initial_mask != NULL) {
        bitvector_copy(&decomp->initial_mask, initial_mask);
        bitvector_copy(&decomp->mask, initial_mask);
    } else {
        bitvector_zero(&decomp->initial_mask);
        bitvector_zero(&decomp->mask);
    }

    /* Initialize diagnostics */
    decomp->mask_inconsistent = 0U;
    decomp->count_f_mismatch = 0U;

    /* Reset state */
    pocket_decompressor_reset(decomp);

    return POCKET_OK;
}


void pocket_decompressor_reset(pocket_decompressor_t *decomp) {
    if (decomp != NULL) {
        decomp->t = 0U;
        bitvector_copy(&decomp->mask, &decomp->initial_mask);
        bitvector_zero(&decomp->prev_output);
        bitvector_zero(&decomp->Xt);
    }
}


int pocket_decompressor_notify_packet_loss(
    pocket_decompressor_t *decomp,
    uint32_t lost_count
) {
    if (decomp == NULL) {
        return POCKET_ERROR_INVALID_ARG;
    }

    if (lost_count == 0U) {
        return POCKET_OK;  /* No loss, nothing to do */
    }

    /*
     * Advance the time index to account for lost packets.
     * This is critical for synchronization with the compressor's time index.
     *
     * Note: After packet loss, the next packet should ideally have:
     * - rt=1 (uncompressed) for full recovery, OR
     * - ft=1 (full mask) for mask recovery
     *
     * If the loss count <= R (minimum robustness), the next packet's Xt
     * window will contain all mask changes, allowing mask synchronization.
     * However, prev_output will be stale unless the next packet is uncompressed.
     */
    decomp->t += lost_count;

    /*
     * Mark prev_output as potentially invalid.
     * The next packet's data should be uncompressed (rt=1) for reliable recovery,
     * or we accept that prediction-based decompression may fail.
     *
     * Per CCSDS 124.0-B-1 Section 3.3.2: "The uncompressed flag, rt, which
     * shall be rt = 1 if t <= Rt" - this ensures the first R+1 packets are
     * always uncompressed, providing natural synchronization points.
     */

    return POCKET_OK;
}

/** @} */ /* End of Decompressor Initialization */

/**
 * @name Packet Decompression
 * @{
 */


int pocket_decompress_packet(
    pocket_decompressor_t *decomp,
    bitreader_t *reader,
    bitvector_t *output
) {
    if ((decomp == NULL) || (reader == NULL) || (output == NULL)) {
        return POCKET_ERROR_INVALID_ARG;
    }

    (void)bitvector_init(output, decomp->F);

    /* Copy previous output as prediction base */
    bitvector_copy(output, &decomp->prev_output);

    /* Clear positive changes tracker */
    bitvector_zero(&decomp->Xt);

    /* ====================================================================
     * Parse hₜ: Mask change information
     * hₜ = RLE(Xₜ) ∥ BIT₄(Vₜ) ∥ eₜ ∥ kₜ ∥ cₜ ∥ ḋₜ
     * ==================================================================== */

    /* Decode RLE(Xₜ) - mask changes */
    bitvector_t Xt;
    int status = pocket_rle_decode(reader, &Xt, decomp->F);
    if (status != POCKET_OK) {
        return status;
    }

    /* Read BIT₄(Vₜ) - effective robustness */
    if (bitreader_remaining(reader) < 4U) {
        return POCKET_ERROR_UNDERFLOW;
    }
    uint32_t vt_raw = bitreader_read_bits(reader, 4U);
    uint8_t Vt = (uint8_t)(vt_raw & 0x0FU);

    /* Process eₜ, kₜ, cₜ if Vₜ > 0 and there are changes */
    int ct = 0;
    size_t change_count = bitvector_hamming_weight(&Xt);

    if ((Vt > 0U) && (change_count > 0U)) {
        /* Read eₜ */
        int et = bitreader_read_bit(reader);
        if (et < 0) {
            return POCKET_ERROR_UNDERFLOW;
        }

        /* Pre-extract positions of set bits in Xt (word-level, much faster than bit-by-bit) */
        size_t change_positions[POCKET_MAX_PACKET_LENGTH];
        size_t num_changes = bitvector_get_set_positions(&Xt, change_positions, change_count);

        if (et == 1) {
            /* Read kₜ - determines positive/negative updates */
            /* kₜ has one bit per change in Xt */
            uint8_t kt_bits[POCKET_MAX_PACKET_LENGTH];

            /* Read kt bits using pre-extracted positions */
            if (bitreader_remaining(reader) < num_changes) {
                return POCKET_ERROR_UNDERFLOW;
            }
            for (size_t idx = 0U; idx < num_changes; idx++) {
                int bit_val = bitreader_read_bit(reader);
                if (bit_val < 0) {
                    return POCKET_ERROR_UNDERFLOW;
                }
                kt_bits[idx] = (bit_val > 0) ? 1U : 0U;
            }

            /* Apply mask updates using pre-extracted positions */
            for (size_t idx = 0U; idx < num_changes; idx++) {
                size_t pos = change_positions[idx];
                /* kt=1 means positive update (mask becomes 0) */
                /* kt=0 means negative update (mask becomes 1) */
                if (kt_bits[idx] != 0U) {
                    bitvector_set_bit(&decomp->mask, pos, 0);
                    bitvector_set_bit(&decomp->Xt, pos, 1);  /* Track positive change */
                } else {
                    bitvector_set_bit(&decomp->mask, pos, 1);
                }
            }

            /* Read cₜ */
            ct = bitreader_read_bit(reader);
            if (ct < 0) {
                return POCKET_ERROR_UNDERFLOW;
            }
        } else {
            /* et = 0: all updates are negative (mask bits become 1) */
            for (size_t idx = 0U; idx < num_changes; idx++) {
                bitvector_set_bit(&decomp->mask, change_positions[idx], 1);
            }
        }
    } else if ((Vt == 0U) && (change_count > 0U)) {
        /* Vt = 0: toggle mask bits at change positions */
        /* Pre-extract positions of set bits in Xt */
        size_t change_positions[POCKET_MAX_PACKET_LENGTH];
        size_t num_changes = bitvector_get_set_positions(&Xt, change_positions, change_count);

        for (size_t idx = 0U; idx < num_changes; idx++) {
            size_t pos = change_positions[idx];
            int current_val = bitvector_get_bit(&decomp->mask, pos);
            int toggled = 0;
            if (current_val == 0) {
                toggled = 1;
            }
            bitvector_set_bit(&decomp->mask, pos, toggled);
        }
    } else {
        /* No changes to apply (change_count == 0) */
    }

    /* Read ḋₜ */
    int dt = bitreader_read_bit(reader);
    if (dt < 0) {
        return POCKET_ERROR_UNDERFLOW;
    }

    /* ====================================================================
     * Parse qₜ: Optional full mask
     * ==================================================================== */

    int rt = 0;

    /* Reset diagnostics flags */
    decomp->mask_inconsistent = 0U;
    decomp->count_f_mismatch = 0U;

    /* dt=1 means both ft=0 and rt=0 (optimization per CCSDS Eq. 13) */
    /* dt=0 means we need to read ft and rt from the stream */

    if (dt == 0) {
        /* Read ft flag */
        int ft = bitreader_read_bit(reader);
        if (ft < 0) {
            return POCKET_ERROR_UNDERFLOW;
        }

        if (ft == 1) {
            /* Save delta-updated mask before full mask replacement (v1.11) */
            bitvector_t delta_mask;
            bitvector_copy(&delta_mask, &decomp->mask);

            /* Full mask follows: decode RLE(M XOR (M<<)) */
            bitvector_t mask_diff;
            status = pocket_rle_decode(reader, &mask_diff, decomp->F);
            if (status != POCKET_OK) {
                return status;
            }

            /* Reverse the horizontal XOR to get the actual mask.
             * HXOR encoding: HXOR[i] = M[i] XOR M[i+1], with HXOR[F-1] = M[F-1]
             * Reversal: start from LSB (position F-1) and work towards MSB (position 0)
             * M[F-1] = HXOR[F-1] (just copy)
             * M[i] = HXOR[i] XOR M[i+1] for i < F-1
             */

            /* Copy LSB bit directly (position F-1 in bitvector) */
            int current = bitvector_get_bit(&mask_diff, decomp->F - 1U);
            bitvector_set_bit(&decomp->mask, decomp->F - 1U, current);

            /* Process remaining bits from F-2 down to 0 */
            for (size_t i = decomp->F - 1U; i > 0U; i--) {
                size_t pos = i - 1U;
                int hxor_bit = bitvector_get_bit(&mask_diff, pos);
                /* M[pos] = HXOR[pos] XOR M[pos+1] = HXOR[pos] XOR current */
                current = hxor_bit ^ current;
                bitvector_set_bit(&decomp->mask, pos, current);
            }

            /* Check delta/full mask consistency (v1.11) */
            if (bitvector_equals(&delta_mask, &decomp->mask) == 0) {
                decomp->mask_inconsistent = 1U;
            }

        }

        /* Read rt flag */
        rt = bitreader_read_bit(reader);
        if (rt < 0) {
            return POCKET_ERROR_UNDERFLOW;
        }
    }

    if (rt == 1) {
        /* Full packet follows: COUNT(F) ∥ Iₜ */
        uint32_t packet_length = 0U;
        status = pocket_count_decode(reader, &packet_length);
        if (status != POCKET_OK) {
            return status;
        }

        /* Check COUNT(F) against expected packet length (diagnostic flag).
         * A mismatch indicates corruption but we still attempt decompression
         * so the harness can decide whether to accept or reject. */
        if (packet_length != (uint32_t)decomp->F) {
            decomp->count_f_mismatch = 1U;
        }

        /* Read full packet */
        if (bitreader_remaining(reader) < decomp->F) {
            return POCKET_ERROR_UNDERFLOW;
        }
        for (size_t i = 0U; i < decomp->F; i++) {
            int bit = bitreader_read_bit(reader);
            if (bit < 0) {
                return POCKET_ERROR_UNDERFLOW;
            }
            bitvector_set_bit(output, i, bit);
        }
    } else {
        /* Compressed: extract unpredictable bits */
        bitvector_t extraction_mask;
        (void)bitvector_init(&extraction_mask, decomp->F);

        if ((ct == 1) && (Vt > 0U)) {
            /* BE(Iₜ, (Xₜ OR Mₜ)) */
            bitvector_or(&extraction_mask, &decomp->mask, &decomp->Xt);
        } else {
            /* BE(Iₜ, Mₜ) */
            bitvector_copy(&extraction_mask, &decomp->mask);
        }

        /* Insert unpredictable bits */
        status = pocket_bit_insert(reader, output, &extraction_mask);
        if (status != POCKET_OK) {
            return status;
        }
    }

    /* ====================================================================
     * Update state for next cycle
     * ==================================================================== */

    bitvector_copy(&decomp->prev_output, output);
    decomp->t++;

    return POCKET_OK;
}


int pocket_decompress(
    pocket_decompressor_t *decomp,
    const uint8_t *input_data,
    size_t input_size,
    uint8_t *output_buffer,
    size_t output_buffer_size,
    size_t *output_size
) {
    if ((decomp == NULL) || (input_data == NULL) ||
        (output_buffer == NULL) || (output_size == NULL)) {
        return POCKET_ERROR_INVALID_ARG;
    }

    /* Reset decompressor */
    pocket_decompressor_reset(decomp);

    /* Initialize bit reader */
    bitreader_t reader;
    bitreader_init(&reader, input_data, input_size * 8U);

    /* Output packet size in bytes */
    size_t packet_bytes = (decomp->F + 7U) / 8U;
    size_t total_output = 0U;

    /* Decompress packets until input exhausted */
    while (bitreader_remaining(&reader) > 0U) {
        bitvector_t output;
        int status = pocket_decompress_packet(decomp, &reader, &output);
        if (status != POCKET_OK) {
            return status;
        }

        /* Check output buffer space */
        if ((total_output + packet_bytes) > output_buffer_size) {
            return POCKET_ERROR_OVERFLOW;
        }

        /* Copy to output buffer */
        (void)bitvector_to_bytes(&output, &output_buffer[total_output], packet_bytes);
        total_output += packet_bytes;

        /* Align to byte boundary for next packet */
        bitreader_align_byte(&reader);
    }

    *output_size = total_output;
    return POCKET_OK;
}

/** @} */ /* End of Packet Decompression */

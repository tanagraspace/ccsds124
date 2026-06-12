/**
 * @file bitvector.c
 * @brief Fixed-length bit vector implementation using 32-bit words.
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
 * This module provides fixed-length bit vector operations optimized for
 * CCSDS 124.0-B-1 compression. Uses 32-bit words with big-endian byte packing to
 * match ESA/ESOC reference implementation.
 *
 * @par Bit Numbering Convention (CCSDS 124.0-B-1 Section 1.6.1)
 * - Bit 0 = LSB (Least Significant Bit)
 * - Bit N-1 = MSB (Most Significant Bit, transmitted first)
 *
 * @par Word Packing (Big-Endian)
 * Within each 32-bit word:
 * - Word[i] = (Byte[4i] << 24) | (Byte[4i+1] << 16) | (Byte[4i+2] << 8) | Byte[4i+3]
 * - Bit 0 = LSB of word, Bit 31 = MSB of word
 *
 * @authors Georges Labrèche <georges@tanagraspace.com> — https://georges.fyi
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#include "ccsds124.h"
#include <string.h>

/**
 * @name Initialization Functions
 * @{
 */


int bitvector_init(bitvector_t *bv, size_t num_bits) {
    int result = CCSDS124_ERROR_INVALID_ARG;

    if (bv != NULL) {
        if ((num_bits > 0U) && (num_bits <= (size_t)CCSDS124_MAX_PACKET_LENGTH)) {
            bv->length = num_bits;
            /* Calculate number of 32-bit words needed */
            size_t num_bytes = (num_bits + 7U) / 8U;
            bv->num_words = (num_bytes + 3U) / 4U;  /* Ceiling division */

            /* Zero all words */
            (void)memset(bv->data, 0, bv->num_words * sizeof(uint32_t));

            result = CCSDS124_OK;
        }
    }

    return result;
}


void bitvector_zero(bitvector_t *bv) {
    if (bv != NULL) {
        (void)memset(bv->data, 0, bv->num_words * sizeof(uint32_t));
    }
}


void bitvector_copy(bitvector_t *dest, const bitvector_t *src) {
    if ((dest != NULL) && (src != NULL)) {
        dest->length = src->length;
        dest->num_words = src->num_words;
        (void)memcpy(dest->data, src->data, src->num_words * sizeof(uint32_t));
    }
}

/** @} */ /* End of Initialization Functions */

/* Note: bitvector_get_bit and bitvector_set_bit are now static inline in ccsds124.h */

/**
 * @name Bitwise Operations
 *
 * Implements bitwise operations as defined in CCSDS 124.0-B-1 Section 1.6.1.
 * All operations work word-by-word for efficiency.
 * @{
 */


void bitvector_xor(bitvector_t *result, const bitvector_t *a, const bitvector_t *b) {
    if ((result != NULL) && (a != NULL) && (b != NULL)) {
        size_t num_words = a->num_words;
        if (b->num_words < num_words) {
            num_words = b->num_words;
        }

        for (size_t i = 0U; i < num_words; i++) {
            result->data[i] = a->data[i] ^ b->data[i];
        }

        result->length = a->length;
        result->num_words = a->num_words;
    }
}


void bitvector_or(bitvector_t *result, const bitvector_t *a, const bitvector_t *b) {
    if ((result != NULL) && (a != NULL) && (b != NULL)) {
        size_t num_words = a->num_words;
        if (b->num_words < num_words) {
            num_words = b->num_words;
        }

        for (size_t i = 0U; i < num_words; i++) {
            result->data[i] = a->data[i] | b->data[i];
        }

        result->length = a->length;
        result->num_words = a->num_words;
    }
}


void bitvector_and(bitvector_t *result, const bitvector_t *a, const bitvector_t *b) {
    if ((result != NULL) && (a != NULL) && (b != NULL)) {
        size_t num_words = a->num_words;
        if (b->num_words < num_words) {
            num_words = b->num_words;
        }

        for (size_t i = 0U; i < num_words; i++) {
            result->data[i] = a->data[i] & b->data[i];
        }

        result->length = a->length;
        result->num_words = a->num_words;
    }
}


void bitvector_not(bitvector_t *result, const bitvector_t *a) {
    if ((result != NULL) && (a != NULL)) {
        for (size_t i = 0U; i < a->num_words; i++) {
            result->data[i] = ~a->data[i];
        }

        /* Mask off unused bits in last word with big-endian packing */
        if (a->num_words > 0U) {
            size_t num_bytes = (a->length + 7U) / 8U;
            size_t bytes_in_last_word = ((num_bytes - 1U) % 4U) + 1U;
            size_t bits_in_last_byte = a->length - ((num_bytes - 1U) * 8U);

            /* Create mask for valid bits in big-endian word */
            uint32_t mask = 0U;
            for (size_t byte = 0U; byte < bytes_in_last_word; byte++) {
                uint8_t byte_mask;
                if (byte == (bytes_in_last_word - 1U)) {
                    /* MSB-aligned: valid bits are at the top of the byte */
                    byte_mask = (uint8_t)(0xFFU << (8U - bits_in_last_byte));
                } else {
                    byte_mask = 0xFFU;
                }
                uint32_t shift_amt = (3U - (uint32_t)byte) * 8U;
                mask |= ((uint32_t)byte_mask) << shift_amt;
            }
            result->data[a->num_words - 1U] &= mask;
        }

        result->length = a->length;
        result->num_words = a->num_words;
    }
}


void bitvector_left_shift(bitvector_t *result, const bitvector_t *a) {
    if ((result != NULL) && (a != NULL)) {
        result->length = a->length;
        result->num_words = a->num_words;

        /* MSB-first with big-endian word packing:
         * Left shift means shift towards MSB (bit 0).
         * In big-endian packing, MSB is at high bits of first word.
         * Word-level: shift each word left by 1, carry high bit from next word. */
        if (a->num_words > 0U) {
            /* Process words from first (MSB) to last (LSB) */
            for (size_t i = 0U; i < (a->num_words - 1U); i++) {
                /* Shift current word left by 1, bring in MSB from next word */
                result->data[i] = (a->data[i] << 1U) | (a->data[i + 1U] >> 31U);
            }
            /* Last word: shift left, LSB becomes 0 */
            result->data[a->num_words - 1U] = a->data[a->num_words - 1U] << 1U;
        }
    }
}


void bitvector_reverse(bitvector_t *result, const bitvector_t *a) {
    if ((result != NULL) && (a != NULL)) {
        bitvector_zero(result);
        result->length = a->length;
        result->num_words = a->num_words;

        /* Reverse bit order */
        for (size_t i = 0U; i < a->length; i++) {
            int bit = bitvector_get_bit(a, i);
            bitvector_set_bit(result, (a->length - 1U) - i, bit);
        }
    }
}

/** @} */ /* End of Bitwise Operations */

/**
 * @name Utility Functions
 * @{
 */


size_t bitvector_hamming_weight(const bitvector_t *bv) {
    size_t count = 0U;

    if (bv != NULL) {
        /* Count '1' bits in each word using popcount intrinsic */
        for (size_t i = 0U; i < bv->num_words; i++) {
            count += (size_t)__builtin_popcount(bv->data[i]);
        }

        /* Adjust for any extra bits in last word */
        size_t num_bytes = (bv->length + 7U) / 8U;
        size_t extra_bits = (num_bytes * 8U) - bv->length;
        if ((extra_bits > 0U) && (bv->num_words > 0U)) {
            /* Count bits in the unused portion of the last word and subtract */
            uint32_t last_word = bv->data[bv->num_words - 1U];
            uint32_t mask = ((1U << (uint32_t)extra_bits) - 1U);  /* Mask for the unused LSBs */
            uint32_t extra_word = last_word & mask;
            count -= (size_t)__builtin_popcount(extra_word);
        }
    }

    return count;
}


int bitvector_equals(const bitvector_t *a, const bitvector_t *b) {
    int result = 0;

    if ((a != NULL) && (b != NULL)) {
        if (a->length == b->length) {
            if (memcmp(a->data, b->data, a->num_words * sizeof(uint32_t)) == 0) {
                result = 1;
            }
        }
    }

    return result;
}

/** @} */ /* End of Utility Functions */

/**
 * @name Byte Conversion Functions
 * @{
 */


int bitvector_from_bytes(bitvector_t *bv, const uint8_t *data, size_t num_bytes) {
    int result = CCSDS124_ERROR_INVALID_ARG;

    if ((bv != NULL) && (data != NULL)) {
        size_t expected_bytes = (bv->length + 7U) / 8U;
        if (num_bytes > expected_bytes) {
            result = CCSDS124_ERROR_OVERFLOW;
        } else {
            /* Zero the array first */
            (void)memset(bv->data, 0, bv->num_words * sizeof(uint32_t));

            /* Pack bytes into 32-bit words (big-endian) */
            int j = 4;  /* Counter for bytes within word (4, 3, 2, 1) */
            uint32_t bytes_to_int = 0U;
            size_t current_word = 0U;

            for (size_t current_byte = 0U; current_byte < num_bytes; current_byte++) {
                j--;
                bytes_to_int |= ((uint32_t)data[current_byte]) << ((unsigned)j * 8U);

                if (j == 0) {
                    /* Word complete - store it */
                    bv->data[current_word] = bytes_to_int;
                    current_word++;
                    bytes_to_int = 0U;
                    j = 4;
                }
            }

            /* Handle incomplete final word */
            if (j < 4) {
                bv->data[current_word] = bytes_to_int;
            }

            result = CCSDS124_OK;
        }
    }

    return result;
}


int bitvector_to_bytes(const bitvector_t *bv, uint8_t *data, size_t num_bytes) {
    int result = CCSDS124_ERROR_INVALID_ARG;

    if ((bv != NULL) && (data != NULL)) {
        size_t expected_bytes = (bv->length + 7U) / 8U;
        if (num_bytes < expected_bytes) {
            result = CCSDS124_ERROR_UNDERFLOW;
        } else {
            /* Extract bytes from 32-bit words (big-endian) */
            size_t byte_index = 0U;
            for (size_t word_index = 0U; (word_index < bv->num_words) && (byte_index < num_bytes); word_index++) {
                uint32_t word = bv->data[word_index];

                /* Extract up to 4 bytes from this word */
                for (int j = 3; (j >= 0) && (byte_index < expected_bytes); j--) {
                    data[byte_index] = (uint8_t)((word >> ((unsigned)j * 8U)) & 0xFFU);
                    byte_index++;
                }
            }

            result = CCSDS124_OK;
        }
    }

    return result;
}

/** @} */ /* End of Byte Conversion Functions */

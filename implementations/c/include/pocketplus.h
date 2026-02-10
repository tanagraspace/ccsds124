/**
 * @file pocketplus.h
 * @brief POCKET+ Compression Library - Public API
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
 * CCSDS 124.0-B-1: Robust Compression of Fixed-Length Housekeeping Data
 *
 * This library implements the POCKET+ lossless compression algorithm
 * standardized by CCSDS for spacecraft housekeeping telemetry.
 *
 * @authors Georges Labrèche <georges@tanagraspace.com> — https://georges.fyi
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 * @see https://opssat.esa.int/pocket-plus/ ESA Reference Implementation
 */

#ifndef POCKET_PLUS_H
#define POCKET_PLUS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @defgroup version Version Information
 * @{
 */
#define POCKET_VERSION_MAJOR 1  /**< Major version number */
#define POCKET_VERSION_MINOR 0  /**< Minor version number */
#define POCKET_VERSION_PATCH 0  /**< Patch version number */
/** @} */

/**
 * @defgroup errors Error Codes
 * @{
 */
#define POCKET_OK                0   /**< Success */
#define POCKET_ERROR_INVALID_ARG -1  /**< Invalid argument */
#define POCKET_ERROR_OVERFLOW    -2  /**< Buffer overflow */
#define POCKET_ERROR_UNDERFLOW   -3  /**< Buffer underflow */
/** @} */

/**
 * @defgroup status Status Codes
 * @{
 */
#define POCKET_STATUS_UNGUARANTEED 1  /**< Decompressed successfully but accuracy not guaranteed */
/** @} */

/**
 * @defgroup config Configuration Constants
 * @{
 */
#ifndef POCKET_MAX_PACKET_LENGTH
#define POCKET_MAX_PACKET_LENGTH 65535U  /**< Maximum packet length in bits (CCSDS max) */
#endif

#define POCKET_MAX_PACKET_BYTES (((POCKET_MAX_PACKET_LENGTH) + 7U) / 8U)  /**< Max packet bytes */
#define POCKET_MAX_ROBUSTNESS 7U     /**< Maximum robustness level (Rₜ) */
#define POCKET_MAX_HISTORY 16U       /**< History depth for change vectors */
#define POCKET_MAX_VT_HISTORY 16U    /**< History size for Vₜ calculation */
#ifndef POCKET_MAX_OUTPUT_BYTES
#define POCKET_MAX_OUTPUT_BYTES ((POCKET_MAX_PACKET_BYTES) * 12U) /**< Max output buffer size (12× for worst-case RLE expansion) */
#endif
/** @} */

/**
 * @defgroup types Type Definitions
 * @{
 */
typedef struct bitvector bitvector_t;           /**< Fixed-length bit vector */
typedef struct bitbuffer bitbuffer_t;           /**< Variable-length output buffer */
typedef struct pocket_compressor pocket_compressor_t;     /**< Compressor state */
typedef struct pocket_decompressor pocket_decompressor_t; /**< Decompressor state (planned) */

/**
 * @brief Compression parameters for a single packet.
 *
 * These flags control the compression behavior per-packet.
 * When using automatic mode (pt_limit > 0, etc.), these are
 * managed internally by the compressor.
 */
typedef struct {
    uint8_t min_robustness;    /**< Rₜ: Minimum robustness level (0-7) */
    uint8_t new_mask_flag;     /**< ṗₜ: Update mask from build vector (0 or 1) */
    uint8_t send_mask_flag;    /**< ḟₜ: Include mask in output (0 or 1) */
    uint8_t uncompressed_flag; /**< ṙₜ: Send uncompressed (0 or 1) */
} pocket_params_t;

/**
 * @brief Decompression result with accuracy guarantee status.
 *
 * Populated by pocket_decompress_packet_checked() to report whether
 * the decompressed output is accuracy-guaranteed per CCSDS 124.0-B-1.
 */
typedef struct {
    uint8_t status;  /**< 0x00=guaranteed, 0x01=unguaranteed */
    uint8_t Vt;      /**< Effective robustness (0-15) */
    uint8_t ft;      /**< Send mask flag (0 or 1) */
    uint8_t rt;      /**< Reference/uncompressed flag (0 or 1) */
} pocket_decompress_result_t;
/** @} */

/**
 * @defgroup bitvector Bit Vector API
 * @brief Fixed-length binary vector operations.
 *
 * Bit vectors are the fundamental data structure for POCKET+ compression.
 * They represent fixed-length binary sequences with operations for
 * bitwise logic, shifting, and counting.
 *
 * @note Uses 32-bit words with big-endian byte packing to match ESA reference.
 * @{
 */

/**
 * @brief Fixed-length bit vector structure.
 *
 * Stores a binary vector of length F bits using 32-bit words.
 * Bit 0 is the LSB, bit F-1 is the MSB.
 */
struct bitvector {
    uint32_t data[((POCKET_MAX_PACKET_BYTES) + 3U) / 4U];  /**< 32-bit word storage */
    size_t length;    /**< Number of bits (F) */
    size_t num_words; /**< Number of 32-bit words used */
};

/**
 * @brief Initialize a bit vector with specified length.
 *
 * Allocates internal storage and sets all bits to zero.
 *
 * @param[out] bv      Bit vector to initialize (caller-allocated)
 * @param[in]  num_bits Number of bits (1 to POCKET_MAX_PACKET_LENGTH)
 * @return POCKET_OK on success, POCKET_ERROR_INVALID_ARG if out of range
 */
int bitvector_init(bitvector_t *bv, size_t num_bits);

/**
 * @brief Set all bits to zero.
 * @param[out] bv Bit vector to clear
 */
void bitvector_zero(bitvector_t *bv);

/**
 * @brief Copy bit vector contents.
 * @param[out] dest Destination bit vector
 * @param[in]  src  Source bit vector
 */
void bitvector_copy(bitvector_t *dest, const bitvector_t *src);

/**
 * @brief Get bit value at position (inline for performance).
 * @param[in] bv  Bit vector
 * @param[in] pos Bit position (0 = LSB, length-1 = MSB)
 * @return Bit value (0 or 1)
 */
static inline int bitvector_get_bit(const bitvector_t *bv, size_t pos) {
    int result = 0;
    if ((bv != NULL) && (pos < bv->length)) {
        size_t word_index = pos >> 5U;
        size_t bit_in_word = 31U - (pos & 31U);
        uint32_t shifted = bv->data[word_index] >> bit_in_word;
        if ((shifted & 1U) != 0U) {
            result = 1;
        }
    }
    return result;
}

/**
 * @brief Set bit value at position (inline for performance).
 * @param[out] bv    Bit vector to modify
 * @param[in]  pos   Bit position (0 = LSB, length-1 = MSB)
 * @param[in]  value Bit value (0 or 1)
 */
static inline void bitvector_set_bit(bitvector_t *bv, size_t pos, int value) {
    if ((bv != NULL) && (pos < bv->length)) {
        size_t word_index = pos >> 5U;
        size_t bit_in_word = 31U - (pos & 31U);
        if (value != 0) {
            bv->data[word_index] |= (1U << bit_in_word);
        } else {
            bv->data[word_index] &= ~(1U << bit_in_word);
        }
    }
}

/**
 * @brief Bitwise XOR operation.
 * @param[out] result Result vector (result = a XOR b)
 * @param[in]  a      First operand
 * @param[in]  b      Second operand
 */
void bitvector_xor(bitvector_t *result, const bitvector_t *a, const bitvector_t *b);

/**
 * @brief Bitwise OR operation.
 * @param[out] result Result vector (result = a OR b)
 * @param[in]  a      First operand
 * @param[in]  b      Second operand
 */
void bitvector_or(bitvector_t *result, const bitvector_t *a, const bitvector_t *b);

/**
 * @brief Bitwise AND operation.
 * @param[out] result Result vector (result = a AND b)
 * @param[in]  a      First operand
 * @param[in]  b      Second operand
 */
void bitvector_and(bitvector_t *result, const bitvector_t *a, const bitvector_t *b);

/**
 * @brief Bitwise NOT operation.
 * @param[out] result Result vector (result = NOT a)
 * @param[in]  a      Operand
 */
void bitvector_not(bitvector_t *result, const bitvector_t *a);

/**
 * @brief Left shift by one bit.
 *
 * Shifts all bits left, inserting 0 at LSB position.
 *
 * @param[out] result Result vector (result = a << 1)
 * @param[in]  a      Operand
 */
void bitvector_left_shift(bitvector_t *result, const bitvector_t *a);

/**
 * @brief Reverse bit order.
 *
 * Reverses the bit order: MSB becomes LSB and vice versa.
 * CCSDS notation: result = <a>
 *
 * @param[out] result Result vector with reversed bits
 * @param[in]  a      Operand
 */
void bitvector_reverse(bitvector_t *result, const bitvector_t *a);

/**
 * @brief Count number of set bits (Hamming weight).
 * @param[in] bv Bit vector
 * @return Number of bits set to 1
 */
size_t bitvector_hamming_weight(const bitvector_t *bv);

/**
 * @brief Compare two bit vectors for equality.
 * @param[in] a First bit vector
 * @param[in] b Second bit vector
 * @return 1 if equal, 0 if different
 */
int bitvector_equals(const bitvector_t *a, const bitvector_t *b);

/**
 * @brief Load bit vector from byte array.
 *
 * Loads bytes in big-endian order (first byte contains MSB bits).
 *
 * @param[out] bv       Bit vector to load into
 * @param[in]  data     Source byte array
 * @param[in]  num_bytes Number of bytes to load
 * @return POCKET_OK on success
 */
int bitvector_from_bytes(bitvector_t *bv, const uint8_t *data, size_t num_bytes);

/**
 * @brief Store bit vector to byte array.
 *
 * Stores bytes in big-endian order (first byte contains MSB bits).
 *
 * @param[in]  bv        Bit vector to store
 * @param[out] data      Destination byte array
 * @param[in]  num_bytes Number of bytes to store
 * @return POCKET_OK on success
 */
int bitvector_to_bytes(const bitvector_t *bv, uint8_t *data, size_t num_bytes);

/** @} */ /* End of bitvector group */

/**
 * @defgroup bitbuffer Bit Buffer API
 * @brief Variable-length output buffer for compressed data.
 *
 * Bit buffers accumulate compressed output bit-by-bit and convert
 * to byte arrays for transmission or storage.
 * @{
 */

/**
 * @brief Variable-length bit buffer structure.
 *
 * Accumulates bits during compression, then converts to bytes.
 * Uses a 32-bit accumulator for efficient bit packing.
 */
struct bitbuffer {
    uint8_t data[POCKET_MAX_OUTPUT_BYTES]; /**< Byte storage */
    size_t num_bits;                       /**< Number of bits written */
    uint32_t acc;                          /**< Bit accumulator (up to 32 bits) */
    size_t acc_len;                        /**< Number of bits in accumulator */
};

/**
 * @brief Initialize bit buffer to empty state.
 * @param[out] bb Bit buffer to initialize
 */
void bitbuffer_init(bitbuffer_t *bb);

/**
 * @brief Clear bit buffer contents.
 * @param[out] bb Bit buffer to clear
 */
void bitbuffer_clear(bitbuffer_t *bb);

/**
 * @brief Append a single bit to buffer.
 * @param[out] bb  Bit buffer
 * @param[in]  bit Bit value (0 or 1)
 * @return POCKET_OK on success, POCKET_ERROR_OVERFLOW if full
 */
int bitbuffer_append_bit(bitbuffer_t *bb, int bit);

/**
 * @brief Append multiple bits from byte array.
 * @param[out] bb       Bit buffer
 * @param[in]  data     Source bytes (MSB first within each byte)
 * @param[in]  num_bits Number of bits to append
 * @return POCKET_OK on success, POCKET_ERROR_OVERFLOW if full
 */
int bitbuffer_append_bits(bitbuffer_t *bb, const uint8_t *data, size_t num_bits);

/**
 * @brief Append multiple bits from a value directly to accumulator.
 *
 * Efficiently appends up to 24 bits from a uint32_t value MSB-first.
 * The top 'num_bits' bits of the value (shifted left) are appended.
 *
 * @param[out] bb       Bit buffer
 * @param[in]  value    Value containing bits to append (left-justified)
 * @param[in]  num_bits Number of bits to append (1-24)
 * @return POCKET_OK on success, POCKET_ERROR_OVERFLOW if full
 */
int bitbuffer_append_value(bitbuffer_t *bb, uint32_t value, size_t num_bits);

/**
 * @brief Append all bits from a bit vector.
 * @param[out] bb Bit buffer
 * @param[in]  bv Bit vector to append
 * @return POCKET_OK on success, POCKET_ERROR_OVERFLOW if full
 */
int bitbuffer_append_bitvector(bitbuffer_t *bb, const bitvector_t *bv);

/**
 * @brief Get number of bits in buffer.
 * @param[in] bb Bit buffer
 * @return Number of bits currently stored
 */
size_t bitbuffer_size(const bitbuffer_t *bb);

/**
 * @brief Convert bit buffer to byte array.
 *
 * Pads final byte with zeros if not byte-aligned.
 *
 * @param[in]  bb        Bit buffer
 * @param[out] data      Destination byte array
 * @param[in]  max_bytes Maximum bytes to write
 * @return Number of bytes written
 */
size_t bitbuffer_to_bytes(const bitbuffer_t *bb, uint8_t *data, size_t max_bytes);

/** @} */ /* End of bitbuffer group */

/**
 * @defgroup mask Mask Update API
 * @brief CCSDS Section 4: Mask vector update equations.
 *
 * These functions implement the core mask prediction logic that
 * identifies which bits are predictable vs. unpredictable.
 * @{
 */

/**
 * @brief Update build vector (CCSDS Equation 6).
 *
 * The build vector accumulates XOR differences between consecutive
 * packets to track which bits have changed.
 *
 * @param[out] build       Updated build vector Bₜ
 * @param[in]  input       Current input Iₜ
 * @param[in]  prev_input  Previous input Iₜ₋₁
 * @param[in]  new_mask_flag ṗₜ flag (1 = reset build)
 * @param[in]  t           Time index (0 = first packet)
 */
void pocket_update_build(
    bitvector_t *build,
    const bitvector_t *input,
    const bitvector_t *prev_input,
    int new_mask_flag,
    size_t t
);

/**
 * @brief Update mask vector (CCSDS Equation 7).
 *
 * The mask vector identifies unpredictable bits (1 = unpredictable).
 *
 * @param[out] mask        Updated mask vector Mₜ
 * @param[in]  input       Current input Iₜ
 * @param[in]  prev_input  Previous input Iₜ₋₁
 * @param[in]  build_prev  Previous build vector Bₜ₋₁
 * @param[in]  new_mask_flag ṗₜ flag (1 = update from build)
 */
void pocket_update_mask(
    bitvector_t *mask,
    const bitvector_t *input,
    const bitvector_t *prev_input,
    const bitvector_t *build_prev,
    int new_mask_flag
);

/**
 * @brief Compute change vector (CCSDS Equation 8).
 *
 * Dₜ = Mₜ XOR Mₜ₋₁. The caller sets prev_mask = mask before
 * calling at t=0 so that D₀ = 0 (no change at initialization).
 *
 * @param[out] change     Change vector Dₜ
 * @param[in]  mask       Current mask Mₜ
 * @param[in]  prev_mask  Previous mask Mₜ₋₁
 * @param[in]  t          Time index (unused, kept for API compatibility)
 */
void pocket_compute_change(
    bitvector_t *change,
    const bitvector_t *mask,
    const bitvector_t *prev_mask,
    size_t t
);

/** @} */ /* End of mask group */

/**
 * @defgroup encoding Encoding API
 * @brief CCSDS Section 5.2: Encoding primitives.
 *
 * These functions implement the three encoding methods used
 * in POCKET+ compressed packets.
 * @{
 */

/**
 * @brief Counter encoding (CCSDS Section 5.2.2, Equation 9).
 *
 * Encodes an integer A as a unary prefix followed by binary suffix.
 * Used for run lengths and counts in compressed output.
 *
 * @param[out] output Bit buffer to append encoded value
 * @param[in]  A      Value to encode (0 to MAX)
 * @return POCKET_OK on success
 */
int pocket_count_encode(bitbuffer_t *output, uint32_t A);

/**
 * @brief Run-length encoding (CCSDS Section 5.2.3, Equation 10).
 *
 * Encodes a bit vector as alternating run lengths of 0s and 1s.
 * Starts with run of 0s from MSB.
 *
 * @param[out] output Bit buffer to append encoded runs
 * @param[in]  input  Bit vector to encode (typically reversed)
 * @return POCKET_OK on success
 */
int pocket_rle_encode(bitbuffer_t *output, const bitvector_t *input);

/**
 * @brief Bit extraction (CCSDS Section 5.2.4, Equation 11).
 *
 * Extracts bits from data where mask bit is 1, in reverse order
 * (MSB to LSB). Used for unpredictable bit encoding.
 *
 * @param[out] output Bit buffer to append extracted bits
 * @param[in]  data   Source data vector
 * @param[in]  mask   Mask indicating which bits to extract
 * @return POCKET_OK on success
 */
int pocket_bit_extract(
    bitbuffer_t *output,
    const bitvector_t *data,
    const bitvector_t *mask
);

/**
 * @brief Bit extraction in forward order (LSB to MSB).
 *
 * Used specifically for kₜ component encoding where forward
 * order is required.
 *
 * @param[out] output Bit buffer to append extracted bits
 * @param[in]  data   Source data vector
 * @param[in]  mask   Mask indicating which bits to extract
 * @return POCKET_OK on success
 */
int pocket_bit_extract_forward(
    bitbuffer_t *output,
    const bitvector_t *data,
    const bitvector_t *mask
);

/** @} */ /* End of encoding group */

/**
 * @defgroup compression Compression API
 * @brief CCSDS Section 5.3: Packet compression.
 *
 * High-level compression interface for processing packets.
 * @{
 */

/**
 * @brief Compressor state structure.
 *
 * Maintains all state needed for sequential packet compression.
 * Must be initialized with pocket_compressor_init() before use.
 */
struct pocket_compressor {
    /** @name Configuration (immutable after init) */
    /** @{ */
    size_t F;                   /**< Input vector length in bits */
    bitvector_t initial_mask;   /**< M₀: Initial mask vector */
    uint8_t robustness;         /**< Rₜ: Base robustness level (0-7) */
    /** @} */

    /** @name State (updated each cycle) */
    /** @{ */
    bitvector_t mask;           /**< Mₜ: Current mask vector */
    bitvector_t prev_mask;      /**< Mₜ₋₁: Previous mask vector */
    bitvector_t build;          /**< Bₜ: Build vector */
    bitvector_t prev_input;     /**< Iₜ₋₁: Previous input packet */
    bitvector_t change_history[POCKET_MAX_HISTORY]; /**< Dₜ₋ᵢ: Change vector history */
    size_t history_index;       /**< Current position in circular buffer */
    /** @} */

    /** @name History for cₜ calculation */
    /** @{ */
    uint8_t new_mask_flag_history[POCKET_MAX_VT_HISTORY]; /**< ṗₜ history */
    size_t flag_history_index;  /**< Index in flag history buffer */
    /** @} */

    /** @name Cycle counter */
    /** @{ */
    size_t t;                   /**< Current time index */
    /** @} */

    /** @name Parameter management (automatic mode) */
    /** @{ */
    int pt_limit;   /**< New mask period (0 = manual) */
    int ft_limit;   /**< Send mask period (0 = manual) */
    int rt_limit;   /**< Uncompressed period (0 = manual) */
    int pt_counter; /**< Countdown to next ṗₜ=1 */
    int ft_counter; /**< Countdown to next ḟₜ=1 */
    int rt_counter; /**< Countdown to next ṙₜ=1 */
    /** @} */

    /** @name Pre-allocated work buffers (avoid per-packet init) */
    /** @{ */
    bitvector_t work_prev_build;   /**< Temporary for prev_build */
    bitvector_t work_change;       /**< Temporary for change vector */
    bitvector_t work_Xt;           /**< Temporary for robustness window */
    bitvector_t work_inverted;     /**< Temporary for inverted mask */
    bitvector_t work_shifted;      /**< Temporary for shifted mask */
    bitvector_t work_diff;         /**< Temporary for mask diff */
    /** @} */
};

/**
 * @brief Initialize compressor state.
 *
 * Sets up the compressor for a new compression session.
 * Use pt_limit/ft_limit/rt_limit > 0 for automatic parameter
 * management, or 0 to control parameters manually via pocket_params_t.
 *
 * @param[out] comp         Compressor to initialize (caller-allocated)
 * @param[in]  F            Input vector length in bits
 * @param[in]  initial_mask M₀ initial mask (NULL = all zeros)
 * @param[in]  robustness   Rₜ base robustness level (0-7)
 * @param[in]  pt_limit     New mask period (0 = manual control)
 * @param[in]  ft_limit     Send mask period (0 = manual control)
 * @param[in]  rt_limit     Uncompressed period (0 = manual control)
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_compressor_init(
    pocket_compressor_t *comp,
    size_t F,
    const bitvector_t *initial_mask,
    uint8_t robustness,
    int pt_limit,
    int ft_limit,
    int rt_limit
);

/**
 * @brief Reset compressor to initial state.
 *
 * Resets time index to 0 and clears all history while
 * preserving configuration (F, robustness, limits).
 *
 * @param[out] comp Compressor to reset
 */
void pocket_compressor_reset(pocket_compressor_t *comp);

/**
 * @brief Compress a single input packet.
 *
 * Compresses one F-bit packet according to CCSDS 124.0-B-1 Section 5.3.
 * Updates compressor state for next packet.
 *
 * @param[in,out] comp   Compressor state (updated in place)
 * @param[in]     input  Input packet Iₜ (must be length F)
 * @param[out]    output Compressed output buffer
 * @param[in]     params Compression parameters (NULL = use automatic)
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_compress_packet(
    pocket_compressor_t *comp,
    const bitvector_t *input,
    bitbuffer_t *output,
    const pocket_params_t *params
);

/**
 * @brief Compress entire input data stream.
 *
 * High-level API that handles:
 * - Splitting input into F-bit packets
 * - Automatic ṗₜ/ḟₜ/ṙₜ parameter management
 * - CCSDS init phase (first Rₜ+1 packets)
 * - Output accumulation with byte padding
 *
 * @param[in,out] comp              Compressor state (must be initialized)
 * @param[in]     input_data        Raw input byte array
 * @param[in]     input_size        Input size in bytes (multiple of F/8)
 * @param[out]    output_buffer     Output buffer (caller-allocated)
 * @param[in]     output_buffer_size Size of output buffer
 * @param[out]    output_size       Actual bytes written
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_compress(
    pocket_compressor_t *comp,
    const uint8_t *input_data,
    size_t input_size,
    uint8_t *output_buffer,
    size_t output_buffer_size,
    size_t *output_size
);

/**
 * @brief Compute robustness window Xₜ.
 *
 * Computes the OR of recent change vectors for robustness encoding:
 * Xₜ = <(Dₜ₋ᴿₜ OR ... OR Dₜ)> where <a> denotes bit reversal.
 *
 * @param[out] Xt            Robustness window (reversed for RLE)
 * @param[in]  comp          Compressor with change history
 * @param[in]  current_change Current change vector Dₜ
 */
void pocket_compute_robustness_window(
    bitvector_t *Xt,
    const pocket_compressor_t *comp,
    const bitvector_t *current_change
);

/**
 * @brief Compute effective robustness Vₜ.
 *
 * Calculates Vₜ = Rₜ + Cₜ where Cₜ counts consecutive iterations
 * without mask changes.
 *
 * @param[in] comp          Compressor with change history
 * @param[in] current_change Current change vector Dₜ
 * @return Effective robustness Vₜ (0-15)
 */
uint8_t pocket_compute_effective_robustness(
    const pocket_compressor_t *comp,
    const bitvector_t *current_change
);

/**
 * @brief Check for positive mask updates (eₜ flag).
 *
 * Determines if any changed bits in Xₜ are predictable
 * (have mask bit = 0), indicating positive updates.
 *
 * @param[in] Xt   Robustness window (reversed)
 * @param[in] mask Current mask Mₜ
 * @return 1 if positive updates exist, 0 otherwise
 */
int pocket_has_positive_updates(
    const bitvector_t *Xt,
    const bitvector_t *mask
);

/**
 * @brief Compute cₜ flag for multiple mask updates.
 *
 * Returns 1 if ṗₜ was set 2+ times in the last Vₜ iterations,
 * indicating the kₜ component is needed in output.
 *
 * @param[in] comp                 Compressor with flag history
 * @param[in] Vt                   Effective robustness level
 * @param[in] current_new_mask_flag Current packet's ṗₜ value
 * @return 1 if multiple updates, 0 otherwise
 */
int pocket_compute_ct_flag(
    const pocket_compressor_t *comp,
    uint8_t Vt,
    int current_new_mask_flag
);

/** @} */ /* End of compression group */

/**
 * @defgroup bitreader Bit Reader API
 * @brief Sequential bit reading from compressed data.
 *
 * The bit reader provides stateful bit-level access to compressed
 * packet data, reading MSB-first within each byte.
 * @{
 */

/**
 * @brief Bit reader structure for sequential reading.
 *
 * Tracks position within a byte buffer for bit-level access.
 * Used during decompression to parse compressed packets.
 */
typedef struct {
    const uint8_t *data;  /**< Pointer to source data buffer */
    size_t num_bits;      /**< Total number of bits available */
    size_t bit_pos;       /**< Current bit position (0 = first bit) */
} bitreader_t;

/**
 * @brief Initialize bit reader.
 *
 * @param[out] reader   Reader to initialize
 * @param[in]  data     Source byte buffer
 * @param[in]  num_bits Number of valid bits in buffer
 */
void bitreader_init(bitreader_t *reader, const uint8_t *data, size_t num_bits);

/**
 * @brief Read a single bit.
 *
 * @param[in,out] reader Bit reader (position advanced)
 * @return Bit value (0 or 1), or -1 if no bits remaining
 */
int bitreader_read_bit(bitreader_t *reader);

/**
 * @brief Read multiple bits as unsigned value.
 *
 * Reads up to 32 bits MSB-first and returns as uint32_t.
 *
 * @param[in,out] reader   Bit reader (position advanced)
 * @param[in]     num_bits Number of bits to read (1-32)
 * @return Unsigned value from bits read
 */
uint32_t bitreader_read_bits(bitreader_t *reader, size_t num_bits);

/**
 * @brief Get current bit position.
 *
 * @param[in] reader Bit reader
 * @return Number of bits already read
 */
size_t bitreader_position(const bitreader_t *reader);

/**
 * @brief Get remaining bits.
 *
 * @param[in] reader Bit reader
 * @return Number of bits remaining to read
 */
size_t bitreader_remaining(const bitreader_t *reader);

/**
 * @brief Skip to next byte boundary.
 *
 * Advances position to start of next byte (for padding).
 *
 * @param[in,out] reader Bit reader (position advanced)
 */
void bitreader_align_byte(bitreader_t *reader);

/** @} */ /* End of bitreader group */

/**
 * @defgroup decoding Decoding API
 * @brief CCSDS Section 5.2: Decoding primitives (inverse of encoding).
 *
 * These functions decode the encoded formats back to original values.
 * @{
 */

/**
 * @brief Counter decoding (inverse of pocket_count_encode).
 *
 * Decodes a COUNT-encoded value from the bit stream.
 * - '0' → 1
 * - '10' → 0 (terminator)
 * - '110' + 5 bits → value + 2
 * - '111' + variable bits → larger values
 *
 * @param[in,out] reader Bit reader (position advanced)
 * @param[out]    value  Decoded value (0 = terminator)
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_count_decode(bitreader_t *reader, uint32_t *value);

/**
 * @brief Run-length decoding (inverse of pocket_rle_encode).
 *
 * Decodes an RLE-encoded bit vector from the bit stream.
 *
 * @param[in,out] reader Bit reader (position advanced)
 * @param[out]    result Decoded bit vector
 * @param[in]     length Expected bit vector length
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_rle_decode(bitreader_t *reader, bitvector_t *result, size_t length);

/**
 * @brief Bit insertion (inverse of pocket_bit_extract).
 *
 * Inserts extracted bits back into a vector at mask positions.
 *
 * @param[in,out] reader Bit reader (position advanced)
 * @param[out]    data   Destination data vector
 * @param[in]     mask   Mask indicating insertion positions
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_bit_insert(bitreader_t *reader, bitvector_t *data, const bitvector_t *mask);

/** @} */ /* End of decoding group */

/**
 * @defgroup decompression Decompression API
 * @brief CCSDS Section 5.3: Packet decompression.
 *
 * High-level decompression interface for processing compressed packets.
 * @{
 */

/**
 * @brief Decompressor state structure.
 *
 * Maintains all state needed for sequential packet decompression.
 * Must be initialized with pocket_decompressor_init() before use.
 */
struct pocket_decompressor {
    /** @name Configuration (immutable after init) */
    /** @{ */
    size_t F;                   /**< Output vector length in bits */
    bitvector_t initial_mask;   /**< M₀: Initial mask vector */
    uint8_t robustness;         /**< Rₜ: Base robustness level (0-7) */
    /** @} */

    /** @name State (updated each cycle) */
    /** @{ */
    bitvector_t mask;           /**< Mₜ: Current mask vector */
    bitvector_t prev_output;    /**< Iₜ₋₁: Previous output packet */
    bitvector_t Xt;             /**< Xₜ: Robustness window (positive changes) */
    /** @} */

    /** @name Cycle counter */
    /** @{ */
    size_t t;                   /**< Current time index */
    /** @} */

    /** @name Diagnostics */
    /** @{ */
    uint8_t mask_inconsistent;  /**< Set when ft=1 full mask differs from delta-updated mask */
    uint8_t count_f_mismatch;   /**< Set when COUNT(F) in rt=1 packet differs from expected F */
    /** @} */

    /** @name Accuracy guarantee tracking */
    /** @{ */
    uint8_t mask_synced;        /**< 1 when decoder mask is known to match encoder mask */
    uint8_t received_status_ring[POCKET_MAX_VT_HISTORY]; /**< Status history: 0x00=guaranteed, 0x01=unguaranteed, 0x02=lost */
    uint8_t received_status_count;  /**< Number of valid entries in status ring */
    uint8_t received_status_index;  /**< Write position in status ring */
    /** @} */
};

/**
 * @brief Initialize decompressor state.
 *
 * Sets up the decompressor for a new decompression session.
 *
 * @param[out] decomp       Decompressor to initialize (caller-allocated)
 * @param[in]  F            Output vector length in bits
 * @param[in]  initial_mask M₀ initial mask (NULL = all zeros)
 * @param[in]  robustness   Rₜ base robustness level (0-7)
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_decompressor_init(
    pocket_decompressor_t *decomp,
    size_t F,
    const bitvector_t *initial_mask,
    uint8_t robustness
);

/**
 * @brief Reset decompressor to initial state.
 *
 * Resets time index to 0 and clears all state while
 * preserving configuration (F, robustness).
 *
 * @param[out] decomp Decompressor to reset
 */
void pocket_decompressor_reset(pocket_decompressor_t *decomp);

/**
 * @brief Notify decompressor of packet loss.
 *
 * Informs the decompressor that one or more packets were lost.
 * The decompressor advances its internal time index and prepares
 * for recovery using the next received packet's robustness info.
 *
 * Per CCSDS 124.0-B-1 Section 2.2: "The Recommended Standard does
 * not provide a mechanism for identifying the number of sequential
 * output binary vectors that were lost. Such mechanisms are assumed
 * to be mission specific." This function provides that mechanism.
 *
 * Recovery behavior:
 * - If lost_count <= effective robustness (Vt), the next packet's
 *   Xt window contains all mask changes, enabling mask recovery.
 * - For full data recovery, the next packet should have rt=1
 *   (uncompressed) to provide a valid prediction base.
 *
 * @param[in,out] decomp     Decompressor state (updated in place)
 * @param[in]     lost_count Number of consecutive packets lost (1+)
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_decompressor_notify_packet_loss(
    pocket_decompressor_t *decomp,
    uint32_t lost_count
);

/**
 * @brief Decompress a single compressed packet.
 *
 * Decompresses one packet according to CCSDS 124.0-B-1 Section 5.3.
 * Updates decompressor state for next packet.
 *
 * @param[in,out] decomp Decompressor state (updated in place)
 * @param[in,out] reader Bit reader positioned at packet start
 * @param[out]    output Decompressed output packet (length F)
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_decompress_packet(
    pocket_decompressor_t *decomp,
    bitreader_t *reader,
    bitvector_t *output
);

/**
 * @brief Decompress a single packet with accuracy guarantee checking.
 *
 * Like pocket_decompress_packet() but additionally tracks mask
 * synchronization and evaluates the accuracy guarantee decision tree
 * per CCSDS 124.0-B-1.
 *
 * If the output is not accuracy-guaranteed, the decompressor state is
 * restored to its pre-call value (except for count_f_mismatch cases
 * where the mask was resynchronized via ft=1).
 *
 * @param[in,out] decomp   Decompressor state (updated in place)
 * @param[in]     data     Compressed packet bytes
 * @param[in]     num_bits Number of valid bits in data
 * @param[out]    output   Decompressed output packet (length F)
 * @param[out]    result   Optional result details (NULL to skip)
 * @return POCKET_OK if guaranteed, POCKET_STATUS_UNGUARANTEED if not,
 *         or negative error code on failure
 */
int pocket_decompress_packet_checked(
    pocket_decompressor_t *decomp,
    const uint8_t *data,
    size_t num_bits,
    bitvector_t *output,
    pocket_decompress_result_t *result
);

/**
 * @brief Discover packet length (F) from a compressed packet.
 *
 * Parses the self-delimiting bitstream structure of a compressed packet
 * to discover the packet length F from reference packets (rt=1) that
 * contain COUNT(F). Non-reference packets cannot reveal F.
 *
 * This is useful when receiving compressed data without out-of-band
 * metadata about the original packet size.
 *
 * @param[in]  data          Compressed packet bytes
 * @param[in]  num_bits      Number of valid bits in data
 * @param[out] packet_length Discovered F value (0 if not discoverable)
 * @return POCKET_OK on success (check packet_length for result),
 *         POCKET_ERROR_INVALID_ARG for NULL pointers or zero bits
 */
int pocket_discover_packet_length(
    const uint8_t *data,
    size_t num_bits,
    uint32_t *packet_length
);

/**
 * @brief Decompress entire compressed data stream.
 *
 * High-level API that handles:
 * - Sequential packet decompression
 * - Byte boundary alignment between packets
 * - Output accumulation
 *
 * @param[in,out] decomp             Decompressor state (must be initialized)
 * @param[in]     input_data         Compressed input byte array
 * @param[in]     input_size         Input size in bytes
 * @param[out]    output_buffer      Output buffer (caller-allocated)
 * @param[in]     output_buffer_size Size of output buffer
 * @param[out]    output_size        Actual bytes written
 * @return POCKET_OK on success, error code otherwise
 */
int pocket_decompress(
    pocket_decompressor_t *decomp,
    const uint8_t *input_data,
    size_t input_size,
    uint8_t *output_buffer,
    size_t output_buffer_size,
    size_t *output_size
);

/** @} */ /* End of decompression group */

/**
 * @defgroup utility Utility API
 * @brief Helper functions.
 * @{
 */

/**
 * @brief Get version string.
 * @return Version string in format "X.Y.Z"
 */
const char* pocket_version_string(void);

/**
 * @brief Get error message for error code.
 * @param[in] error_code Error code from API function
 * @return Human-readable error message
 */
const char* pocket_error_string(int error_code);

/** @} */ /* End of utility group */

#endif /* POCKET_PLUS_H */

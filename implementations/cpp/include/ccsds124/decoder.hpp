/**
 * @file decoder.hpp
 * @brief CCSDS 124.0-B-1 decoding functions (COUNT, RLE, bit insert).
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
 * Implements CCSDS 124.0-B-1 decoding (inverse of Section 5.2):
 * - Counter Decoding (COUNT) - inverse of Section 5.2.2, Equation 9
 * - Run-Length Decoding (RLE) - inverse of Section 5.2.3, Equation 10
 * - Bit Insertion - inverse of Section 5.2.4, Equation 11
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef CCSDS124_DECODER_HPP
#define CCSDS124_DECODER_HPP

#include "bitreader.hpp"
#include "bitvector.hpp"
#include "config.hpp"
#include "error.hpp"

namespace ccsds124 {

/**
 * @brief Counter decoding (inverse of CCSDS Section 5.2.2, Equation 9).
 *
 * Decodes a COUNT-encoded value from a bit reader.
 * - '0' → A = 1
 * - '10' → terminator (A = 0)
 * - '110' + 5 bits → A = value + 2
 * - '111' + variable bits → A = value + 2
 *
 * @param reader Bit reader positioned at COUNT-encoded data
 * @param[out] value Decoded value (0 for terminator, 1+ for values)
 * @return Error::Ok on success, Error::Underflow if not enough bits
 */
inline Error count_decode(BitReader& reader, std::uint32_t& value) noexcept {
    if (reader.remaining() == 0) {
        return Error::Underflow;
    }

    // Read first bit
    int bit0 = reader.read_bit();
    if (bit0 < 0) {
        return Error::Underflow;
    }

    if (bit0 == 0) {
        // Case 1: '0' → value is 1
        value = 1;
        return Error::Ok;
    }

    // First bit is 1, read second bit
    int bit1 = reader.read_bit();
    if (bit1 < 0) {
        return Error::Underflow;
    }

    if (bit1 == 0) {
        // Case 2: '10' → terminator (value 0)
        value = 0;
        return Error::Ok;
    }

    // First two bits are 11, read third bit
    int bit2 = reader.read_bit();
    if (bit2 < 0) {
        return Error::Underflow;
    }

    if (bit2 == 0) {
        // Case 3: '110' + 5 bits → value + 2
        std::uint32_t raw = reader.read_bits(5);
        value = raw + 2;
        return Error::Ok;
    }

    // Case 4: '111' + variable bits
    // Count zeros to determine field size
    std::size_t size = 0;
    int next_bit;
    do {
        next_bit = reader.read_bit();
        if (next_bit < 0) {
            return Error::Underflow;
        }
        ++size;
    } while (next_bit == 0 && reader.remaining() > 0);

    // Size of value field is size + 5
    std::size_t value_bits = size + 5;

    // The '1' bit we just read is part of the value, so we need to account for it
    // Read the remaining bits (value_bits - 1) and combine with the leading 1
    std::uint32_t raw = 1; // Start with the leading 1
    for (std::size_t i = 0; i < value_bits - 1; ++i) {
        int bit = reader.read_bit();
        if (bit < 0) {
            return Error::Underflow;
        }
        raw = (raw << 1) | (static_cast<std::uint32_t>(bit) & 1U);
    }

    value = raw + 2;
    return Error::Ok;
}

/**
 * @brief Run-length decoding (inverse of CCSDS Section 5.2.3, Equation 10).
 *
 * Decodes RLE-encoded data back to a BitVector.
 *
 * @tparam N BitVector length
 * @param reader Bit reader positioned at RLE-encoded data
 * @param[out] result Decoded bit vector
 * @return Error::Ok on success
 */
template <std::size_t N> Error rle_decode(BitReader& reader, BitVector<N>& result) noexcept {
    // Initialize result to all zeros
    result.zero();

    // Start from end of vector (matching RLE encoding which processes LSB to MSB)
    std::size_t bit_position = N;

    // Read COUNT values until terminator
    std::uint32_t delta = 0;
    auto status = count_decode(reader, delta);

    while (status == Error::Ok && delta != 0) {
        // Delta represents (count of zeros + 1). A delta beyond the
        // remaining bit position means the encoding is invalid for this
        // vector length (GOTCHAS #20): reject it instead of silently
        // skipping.
        if (delta > bit_position) {
            return Error::Overflow;
        }
        bit_position -= delta;
        // Set the bit at this position
        result.set_bit(bit_position, 1);

        // Read next delta
        status = count_decode(reader, delta);
    }

    return status;
}

/**
 * @brief Bit insertion (inverse of CCSDS Section 5.2.4, Equation 11).
 *
 * Inserts bits from reader at positions where mask has '1' bits.
 * This is the inverse of bit_extract.
 *
 * @tparam N BitVector length
 * @param reader Bit reader with bits to insert
 * @param[in,out] data Data vector to insert bits into
 * @param mask Mask indicating where to insert
 * @return Error::Ok on success
 */
template <std::size_t N>
Error bit_insert(BitReader& reader, BitVector<N>& data, const BitVector<N>& mask) noexcept {
    std::size_t hamming = mask.hamming_weight();

    if (hamming == 0) {
        // No bits to insert
        return Error::Ok;
    }

    // GOTCHAS #20: fail on underflow instead of silently skipping positions
    if (reader.remaining() < hamming) {
        return Error::Underflow;
    }

    // Insert bits in reverse order (LSB to MSB, matching BE extraction)
    // Iterate by set bits using __builtin_ctz for efficiency
    for (int word = static_cast<int>(BitVector<N>::NUM_WORDS) - 1; word >= 0; --word) {
        std::uint32_t mask_word = mask.data()[word];

        // Loop bounded by popcount - guarantees termination
        for (int bits_remaining = __builtin_popcount(mask_word); bits_remaining > 0;
             --bits_remaining) {
            // Find LSB position using extract_lsb helper
            int bit_pos_in_word = detail::extract_lsb(mask_word);
            std::size_t global_pos =
                (static_cast<std::size_t>(word) * 32) + static_cast<std::size_t>(bit_pos_in_word);

            if (global_pos < N) {
                int bit = reader.read_bit();
                if (bit < 0) {
                    return Error::Underflow;
                }
                data.set_bit(global_pos, bit);
            }
        }
    }

    return Error::Ok;
}

/**
 * @brief Bit insertion with forward order.
 *
 * Inserts bits from reader at positions where mask has '1' bits,
 * in forward order (MSB to LSB).
 *
 * @tparam N BitVector length
 * @param reader Bit reader with bits to insert
 * @param[in,out] data Data vector to insert bits into
 * @param mask Mask indicating where to insert
 * @return Error::Ok on success
 */
template <std::size_t N>
Error bit_insert_forward(BitReader& reader, BitVector<N>& data, const BitVector<N>& mask) noexcept {
    // GOTCHAS #20: fail on underflow instead of silently skipping positions
    if (reader.remaining() < mask.hamming_weight()) {
        return Error::Underflow;
    }

    // Insert bits in forward order (MSB to LSB)
    // Iterate by set bits using __builtin_clz for efficiency
    for (std::size_t word = 0; word < BitVector<N>::NUM_WORDS; ++word) {
        std::uint32_t mask_word = mask.data()[word];

        // Loop bounded by popcount - guarantees termination
        for (int bits_remaining = __builtin_popcount(mask_word); bits_remaining > 0;
             --bits_remaining) {
            // Find MSB position using extract_msb helper
            int clz = detail::extract_msb(mask_word);
            std::size_t global_pos = (word * 32) + static_cast<std::size_t>(clz);

            if (global_pos < N) {
                int bit = reader.read_bit();
                if (bit < 0) {
                    return Error::Underflow;
                }
                data.set_bit(global_pos, bit);
            }
        }
    }

    return Error::Ok;
}

} // namespace ccsds124

#endif // CCSDS124_DECODER_HPP

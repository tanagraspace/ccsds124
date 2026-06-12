/**
 * @file encoder.hpp
 * @brief CCSDS 124.0-B-1 encoding functions (COUNT, RLE, BE).
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
 * Implements CCSDS 124.0-B-1 Section 5.2 encoding schemes:
 * - Counter Encoding (COUNT) - Section 5.2.2, Table 5-1, Equation 9
 * - Run-Length Encoding (RLE) - Section 5.2.3, Equation 10
 * - Bit Extraction (BE) - Section 5.2.4, Equation 11
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef CCSDS124_ENCODER_HPP
#define CCSDS124_ENCODER_HPP

#include "bitbuffer.hpp"
#include "bitvector.hpp"
#include "config.hpp"
#include "error.hpp"

namespace ccsds124 {

/**
 * @brief Pre-computed COUNT encodings for values 1-33.
 */
namespace detail {
inline constexpr std::uint8_t COUNT_VALUES[34] = {
    0U,    0U,                                              // 0: unused, 1: '0'
    0xC0U, 0xC1U, 0xC2U, 0xC3U, 0xC4U, 0xC5U, 0xC6U, 0xC7U, // 2-9
    0xC8U, 0xC9U, 0xCAU, 0xCBU, 0xCCU, 0xCDU, 0xCEU, 0xCFU, // 10-17
    0xD0U, 0xD1U, 0xD2U, 0xD3U, 0xD4U, 0xD5U, 0xD6U, 0xD7U, // 18-25
    0xD8U, 0xD9U, 0xDAU, 0xDBU, 0xDCU, 0xDDU, 0xDEU, 0xDFU  // 26-33
};
inline constexpr std::uint8_t COUNT_BITS[34] = {
    0U, 1U,                         // 0: unused, 1: 1 bit
    8U, 8U, 8U, 8U, 8U, 8U, 8U, 8U, // 2-9: 8 bits each
    8U, 8U, 8U, 8U, 8U, 8U, 8U, 8U, // 10-17
    8U, 8U, 8U, 8U, 8U, 8U, 8U, 8U, // 18-25
    8U, 8U, 8U, 8U, 8U, 8U, 8U, 8U  // 26-33
};
} // namespace detail

/**
 * @brief Counter encoding (CCSDS Section 5.2.2, Equation 9).
 *
 * Encodes an integer A as a unary prefix followed by binary suffix.
 * - A = 1 → '0'
 * - 2 ≤ A ≤ 33 → '110' ∥ BIT₅(A-2)
 * - A ≥ 34 → '111' ∥ BIT_E(A-2)
 *
 * @tparam MaxBytes BitBuffer capacity
 * @param output Bit buffer to append encoded value
 * @param A Value to encode (1 to 65535)
 * @return Error::Ok on success
 */
template <std::size_t MaxBytes>
Error count_encode(BitBuffer<MaxBytes>& output, std::uint32_t A) noexcept {
    if (A == 0 || A > 65535) [[unlikely]] {
        return Error::InvalidArg;
    }

    // Fast path: Most values are in 2-33 range
    if (A <= 33) [[likely]] {
        if (A == 1) {
            // Case 1: A = 1 → '0'
            return output.append_bit(0);
        }
        // Case 2: use lookup table for A=2-33
        return output.append_value(static_cast<std::uint32_t>(detail::COUNT_VALUES[A]),
                                   static_cast<std::size_t>(detail::COUNT_BITS[A]));
    }

    // Case 3: A ≥ 34 → '111' ∥ BIT_E(A-2)
    // Append '111' prefix as single value
    auto result = output.append_value(0b111U, 3);
    if (result != Error::Ok)
        return result;

    // Calculate E = 2⌊log₂(A-2)+1⌋ - 6
    std::uint32_t value = A - 2;
    int highest_bit = 31 - __builtin_clz(value);
    int E = (2 * (highest_bit + 1)) - 6;

    // Append BIT_E(A-2) MSB-first as single value
    return output.append_value(value, static_cast<std::size_t>(E));
}

/**
 * @brief Run-length encoding (CCSDS Section 5.2.3, Equation 10).
 *
 * RLE(a) = COUNT(C₀) ∥ COUNT(C₁) ∥ ... ∥ COUNT(C_{H(a)-1}) ∥ '10'
 *
 * @tparam MaxBytes BitBuffer capacity
 * @tparam N BitVector length
 * @param output Bit buffer to append encoded runs
 * @param input Bit vector to encode
 * @return Error::Ok on success
 */
template <std::size_t MaxBytes, std::size_t N>
Error rle_encode(BitBuffer<MaxBytes>& output, const BitVector<N>& input) noexcept {
    // Start from the end of the vector
    int old_bit_position = static_cast<int>(N);

    // Process words in reverse order (from high to low)
    for (int word = static_cast<int>(BitVector<N>::NUM_WORDS) - 1; word >= 0; --word) {
        std::uint32_t word_data = input.data()[word];

        // Process all set bits in this word
        while (word_data != 0) {
            // Find LSB position using __builtin_ctz (avoids UB with signed negation)
            int trailing_zeros = __builtin_ctz(word_data);
            std::uint32_t lsb = 1U << static_cast<unsigned>(trailing_zeros);

            // Convert to bit position from MSB (big-endian ordering)
            int bit_position_in_word = 31 - trailing_zeros;

            // Calculate global bit position
            int new_bit_position = (word * 32) + bit_position_in_word;

            // Calculate delta (number of zeros + 1)
            int delta = old_bit_position - new_bit_position;

            // Encode the count
            auto result = count_encode(output, static_cast<std::uint32_t>(delta));
            if (result != Error::Ok)
                return result;

            // Update old position for next iteration
            old_bit_position = new_bit_position;

            // Clear the processed bit
            word_data ^= lsb;
        }
    }

    // Append terminator '10' as single value
    return output.append_value(0b10U, 2);
}

/**
 * @brief Bit extraction in reverse order (CCSDS Section 5.2.4, Equation 11).
 *
 * BE(a, b) = a_{g_{H(b)-1}} ∥ ... ∥ a_{g₁} ∥ a_{g₀}
 *
 * @tparam MaxBytes BitBuffer capacity
 * @tparam N BitVector length
 * @param output Bit buffer to append extracted bits
 * @param data Source data vector
 * @param mask Mask indicating which bits to extract
 * @return Error::Ok on success
 */
template <std::size_t MaxBytes, std::size_t N>
Error bit_extract(BitBuffer<MaxBytes>& output, const BitVector<N>& data,
                  const BitVector<N>& mask) noexcept {
    // Batch accumulator for append_value (max 24 bits)
    std::uint32_t batch_value = 0;
    std::size_t batch_count = 0;

    // Process words in REVERSE order (high to low)
    for (int word = static_cast<int>(BitVector<N>::NUM_WORDS) - 1; word >= 0; --word) {
        std::uint32_t mask_word = mask.data()[word];
        std::uint32_t data_word = data.data()[word];

        // Loop bounded by popcount - guarantees termination
        for (int bits_remaining = __builtin_popcount(mask_word); bits_remaining > 0;
             --bits_remaining) {
            // Find LSB position using __builtin_ctz (avoids UB with signed negation)
            int trailing_zeros = __builtin_ctz(mask_word);
            std::uint32_t lsb = 1U << static_cast<unsigned>(trailing_zeros);

            // Convert to bit position from MSB (big-endian ordering)
            int bit_pos_in_word = 31 - trailing_zeros;

            // Check if this bit is within the valid length
            int global_pos = (word * 32) + bit_pos_in_word;
            if (static_cast<std::size_t>(global_pos) < N) {
                // Extract data bit and accumulate in batch
                int bit = (data_word & lsb) != 0 ? 1 : 0;
                batch_value = (batch_value << 1) | static_cast<std::uint32_t>(bit);
                ++batch_count;

                // Flush when batch is full (24 bits max for append_value)
                if (batch_count == 24) {
                    auto result = output.append_value(batch_value, 24);
                    if (result != Error::Ok)
                        return result;
                    batch_value = 0;
                    batch_count = 0;
                }
            }

            // Clear processed bit
            mask_word &= mask_word - 1;
        }
    }

    // Flush remaining bits in batch
    if (batch_count > 0) {
        auto result = output.append_value(batch_value, batch_count);
        if (result != Error::Ok)
            return result;
    }

    return Error::Ok;
}

/**
 * @brief Bit extraction in forward order.
 *
 * Used specifically for kₜ component encoding where forward order is required.
 *
 * @tparam MaxBytes BitBuffer capacity
 * @tparam N BitVector length
 * @param output Bit buffer to append extracted bits
 * @param data Source data vector
 * @param mask Mask indicating which bits to extract
 * @return Error::Ok on success
 */
template <std::size_t MaxBytes, std::size_t N>
Error bit_extract_forward(BitBuffer<MaxBytes>& output, const BitVector<N>& data,
                          const BitVector<N>& mask) noexcept {
    // Batch accumulator for append_value (max 24 bits)
    std::uint32_t batch_value = 0;
    std::size_t batch_count = 0;

    // Process words in FORWARD order (low to high)
    for (std::size_t word = 0; word < BitVector<N>::NUM_WORDS; ++word) {
        std::uint32_t mask_word = mask.data()[word];
        std::uint32_t data_word = data.data()[word];

        // Loop bounded by popcount - guarantees termination
        for (int bits_remaining = __builtin_popcount(mask_word); bits_remaining > 0;
             --bits_remaining) {
            // Find MSB position using extract_msb helper
            int clz = detail::extract_msb(mask_word);

            std::size_t global_pos = (word * 32) + static_cast<std::size_t>(clz);

            if (global_pos < N) {
                // Extract data bit and accumulate in batch
                std::uint32_t bit_mask = 1U << (31U - static_cast<std::uint32_t>(clz));
                int bit = (data_word & bit_mask) != 0 ? 1 : 0;
                batch_value = (batch_value << 1) | static_cast<std::uint32_t>(bit);
                ++batch_count;

                // Flush when batch is full (24 bits max for append_value)
                if (batch_count == 24) {
                    auto result = output.append_value(batch_value, 24);
                    if (result != Error::Ok)
                        return result;
                    batch_value = 0;
                    batch_count = 0;
                }
            }
        }
    }

    // Flush remaining bits in batch
    if (batch_count > 0) {
        auto result = output.append_value(batch_value, batch_count);
        if (result != Error::Ok)
            return result;
    }

    return Error::Ok;
}

/**
 * @brief Bit extraction with different length vectors (for flexibility).
 */
template <std::size_t MaxBytes, std::size_t N1, std::size_t N2>
Error bit_extract(BitBuffer<MaxBytes>& output, const BitVector<N1>& data,
                  const BitVector<N2>& mask) noexcept {
    if constexpr (N1 != N2) {
        return Error::InvalidArg;
    } else {
        return bit_extract<MaxBytes, N1>(output, data, mask);
    }
}

} // namespace ccsds124

#endif // CCSDS124_ENCODER_HPP

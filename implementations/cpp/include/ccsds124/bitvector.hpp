/**
 * @file bitvector.hpp
 * @brief Fixed-length bit vector with static allocation.
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
 * - Bit 0 = MSB (transmitted first)
 * - Bit N-1 = LSB (transmitted last)
 *
 * @par Word Packing (Big-Endian)
 * Within each 32-bit word:
 * - Bit 0 at position 31 of word 0 (most significant)
 * - Bit 31 at position 0 of word 0
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef CCSDS124_BITVECTOR_HPP
#define CCSDS124_BITVECTOR_HPP

#include <array>
#include <cstring>

#include "config.hpp"

namespace ccsds124 {

namespace detail {

/**
 * @brief Extract and clear the most significant set bit from a word.
 *
 * This helper makes bit iteration loops analyzable by static analysis tools.
 * The loop termination is bounded by popcount (number of set bits).
 *
 * @param word Reference to the word (will be modified to clear the MSB)
 * @return Position of the MSB (0-31), or -1 if word was zero
 */
inline int extract_msb(std::uint32_t& word) noexcept {
    if (word == 0) {
        return -1;
    }
    int clz = __builtin_clz(word);
    word &= ~(1U << (31U - static_cast<std::uint32_t>(clz)));
    return clz;
}

/**
 * @brief Extract and clear the least significant set bit from a word.
 *
 * @param word Reference to the word (will be modified to clear the LSB)
 * @return Position of the LSB from MSB (0-31), or -1 if word was zero
 */
inline int extract_lsb(std::uint32_t& word) noexcept {
    if (word == 0) {
        return -1;
    }
    int ctz = __builtin_ctz(word);
    word &= word - 1; // Clear LSB (standard idiom)
    return 31 - ctz;  // Convert to position from MSB
}

} // namespace detail

/**
 * @brief Fixed-length bit vector with compile-time size.
 *
 * @tparam N Number of bits in the vector
 *
 * Uses static allocation only - no heap allocation. Suitable for
 * embedded systems with -fno-exceptions -fno-rtti.
 */
template <std::size_t N> class BitVector {
public:
    /// Number of 32-bit words needed
    static constexpr std::size_t NUM_WORDS = (N + 31) / 32;

    /// Number of bytes needed
    static constexpr std::size_t NUM_BYTES = (N + 7) / 8;

    /**
     * @brief Default constructor - initializes all bits to zero.
     */
    constexpr BitVector() noexcept : data_{} {}

    /**
     * @brief Get the number of bits.
     * @return Number of bits in the vector
     */
    [[nodiscard]] constexpr std::size_t length() const noexcept {
        return N;
    }

    /**
     * @brief Get the number of 32-bit words.
     * @return Number of words used for storage
     */
    [[nodiscard]] constexpr std::size_t num_words() const noexcept {
        return NUM_WORDS;
    }

    /**
     * @brief Set all bits to zero.
     */
    void zero() noexcept {
        data_.fill(0);
    }

    /**
     * @brief Get bit value at position.
     *
     * @param pos Bit position (0 = MSB, N-1 = LSB)
     * @return Bit value (0 or 1)
     */
    [[nodiscard]] inline int get_bit(std::size_t pos) const noexcept {
        if (pos >= N) [[unlikely]]
            return 0;
        return get_bit_unchecked(pos);
    }

    /**
     * @brief Get bit value at position without bounds checking.
     *
     * @warning Caller must ensure pos < N. Undefined behavior if pos >= N.
     * @param pos Bit position (0 = MSB, N-1 = LSB)
     * @return Bit value (0 or 1)
     */
    [[nodiscard]] inline int get_bit_unchecked(std::size_t pos) const noexcept {
        std::size_t word_idx = pos >> 5;
        std::size_t bit_in_word = 31U - (pos & 31U);
        return static_cast<int>((data_[word_idx] >> bit_in_word) & 1U);
    }

    /**
     * @brief Set bit value at position.
     *
     * @param pos Bit position (0 = MSB, N-1 = LSB)
     * @param value Bit value (0 or 1)
     */
    inline void set_bit(std::size_t pos, int value) noexcept {
        if (pos >= N) [[unlikely]]
            return;
        set_bit_unchecked(pos, value);
    }

    /**
     * @brief Set bit value at position without bounds checking.
     *
     * @warning Caller must ensure pos < N. Undefined behavior if pos >= N.
     * @param pos Bit position (0 = MSB, N-1 = LSB)
     * @param value Bit value (0 or 1)
     */
    inline void set_bit_unchecked(std::size_t pos, int value) noexcept {
        std::size_t word_idx = pos >> 5;
        std::size_t bit_in_word = 31U - (pos & 31U);

        if (value) {
            data_[word_idx] |= (1U << bit_in_word);
        } else {
            data_[word_idx] &= ~(1U << bit_in_word);
        }
    }

    /**
     * @brief Copy from another BitVector.
     * @param src Source vector
     */
    void copy_from(const BitVector& src) noexcept {
        data_ = src.data_;
    }

    /**
     * @brief Compute XOR of two vectors.
     * @param a First operand
     * @param b Second operand
     */
    void xor_of(const BitVector& a, const BitVector& b) noexcept {
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            data_[i] = a.data_[i] ^ b.data_[i];
        }
    }

    /**
     * @brief Compute OR of two vectors.
     * @param a First operand
     * @param b Second operand
     */
    void or_of(const BitVector& a, const BitVector& b) noexcept {
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            data_[i] = a.data_[i] | b.data_[i];
        }
    }

    /**
     * @brief Compute AND of two vectors.
     * @param a First operand
     * @param b Second operand
     */
    void and_of(const BitVector& a, const BitVector& b) noexcept {
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            data_[i] = a.data_[i] & b.data_[i];
        }
    }

    /**
     * @brief Compute NOT of a vector.
     * @param a Operand
     */
    void not_of(const BitVector& a) noexcept {
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            data_[i] = ~a.data_[i];
        }
        mask_unused_bits();
    }

    /**
     * @brief Compute left shift by 1 of a vector.
     * @param a Operand
     */
    void left_shift_of(const BitVector& a) noexcept {
        if constexpr (NUM_WORDS > 0) {
            for (std::size_t i = 0; i < NUM_WORDS - 1; ++i) {
                data_[i] = (a.data_[i] << 1) | (a.data_[i + 1] >> 31);
            }
            data_[NUM_WORDS - 1] = a.data_[NUM_WORDS - 1] << 1;
        }
    }

    /**
     * @brief Compute bit reversal of a vector.
     * @param a Operand
     */
    void reverse_of(const BitVector& a) noexcept {
        // Reverse word order and reverse bits within each word
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            std::uint32_t word = a.data_[NUM_WORDS - 1 - i];
            // Reverse bits within word using efficient swaps
            word = ((word & 0x55555555U) << 1) | ((word >> 1) & 0x55555555U);
            word = ((word & 0x33333333U) << 2) | ((word >> 2) & 0x33333333U);
            word = ((word & 0x0F0F0F0FU) << 4) | ((word >> 4) & 0x0F0F0F0FU);
            word = __builtin_bswap32(word); // Reverse bytes
            data_[i] = word;
        }

        // Handle non-32-bit-aligned size: shift to account for unused bits
        constexpr std::size_t extra_bits = (NUM_WORDS * 32) - N;
        if constexpr (extra_bits > 0) {
            // Shift entire array left by extra_bits to align
            for (std::size_t i = 0; i < NUM_WORDS - 1; ++i) {
                data_[i] = (data_[i] << extra_bits) | (data_[i + 1] >> (32 - extra_bits));
            }
            data_[NUM_WORDS - 1] <<= extra_bits;
        }
    }

    /**
     * @brief XOR in-place with another vector.
     * @param other Operand
     */
    void xor_with(const BitVector& other) noexcept {
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            data_[i] ^= other.data_[i];
        }
    }

    /**
     * @brief Compute XOR of two vectors (result = a ^ b).
     * @param a First operand
     * @param b Second operand
     */
    void xor_with(const BitVector& a, const BitVector& b) noexcept {
        xor_of(a, b);
    }

    /**
     * @brief OR in-place with another vector.
     * @param other Operand
     */
    void or_with(const BitVector& other) noexcept {
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            data_[i] |= other.data_[i];
        }
    }

    /**
     * @brief Compute OR of two vectors (result = a | b).
     * @param a First operand
     * @param b Second operand
     */
    void or_with(const BitVector& a, const BitVector& b) noexcept {
        or_of(a, b);
    }

    /**
     * @brief OR in-place with another vector (alias for or_with).
     * @param other Operand
     */
    void or_in_place(const BitVector& other) noexcept {
        or_with(other);
    }

    /**
     * @brief AND in-place with another vector.
     * @param other Operand
     */
    void and_with(const BitVector& other) noexcept {
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            data_[i] &= other.data_[i];
        }
    }

    /**
     * @brief Invert all bits in-place.
     */
    void invert() noexcept {
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            data_[i] = ~data_[i];
        }
        mask_unused_bits();
    }

    /**
     * @brief Left shift by 1 in-place.
     */
    void left_shift() noexcept {
        if constexpr (NUM_WORDS > 0) {
            for (std::size_t i = 0; i < NUM_WORDS - 1; ++i) {
                data_[i] = (data_[i] << 1) | (data_[i + 1] >> 31);
            }
            data_[NUM_WORDS - 1] <<= 1;
        }
    }

    /**
     * @brief Compute left shift of another vector (result = src << 1).
     * @param src Source operand
     */
    void left_shift(const BitVector& src) noexcept {
        left_shift_of(src);
    }

    /**
     * @brief Count number of set bits (Hamming weight).
     * @return Number of bits set to 1
     */
    [[nodiscard]] std::size_t hamming_weight() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < NUM_WORDS; ++i) {
            count += static_cast<std::size_t>(__builtin_popcount(data_[i]));
        }

        // Adjust for unused bits in last word
        constexpr std::size_t extra_bits = (NUM_WORDS * 32) - N;
        if constexpr (extra_bits > 0) {
            word_t last_word = data_[NUM_WORDS - 1];
            word_t mask = (1U << extra_bits) - 1U;
            count -= static_cast<std::size_t>(__builtin_popcount(last_word & mask));
        }

        return count;
    }

    /**
     * @brief Compare for equality.
     * @param other Other vector
     * @return true if equal
     */
    [[nodiscard]] bool operator==(const BitVector& other) const noexcept {
        return data_ == other.data_;
    }

    /**
     * @brief Compare for inequality.
     * @param other Other vector
     * @return true if not equal
     */
    [[nodiscard]] bool operator!=(const BitVector& other) const noexcept {
        return data_ != other.data_;
    }

    /**
     * @brief Load from byte array (big-endian).
     *
     * @param bytes Source byte array
     * @param num_bytes Number of bytes to load
     */
    void from_bytes(const std::uint8_t* bytes, std::size_t num_bytes) noexcept {
        zero();

        std::size_t max_bytes = (num_bytes < NUM_BYTES) ? num_bytes : NUM_BYTES;
        std::size_t word_idx = 0;
        int byte_in_word = 4;
        word_t word_acc = 0;

        for (std::size_t i = 0; i < max_bytes; ++i) {
            --byte_in_word;
            word_acc |= static_cast<word_t>(bytes[i]) << (byte_in_word * 8);

            if (byte_in_word == 0) {
                data_[word_idx] = word_acc;
                ++word_idx;
                word_acc = 0;
                byte_in_word = 4;
            }
        }

        // Handle incomplete final word
        if (byte_in_word < 4) {
            data_[word_idx] = word_acc;
        }
    }

    /**
     * @brief Store to byte array (big-endian).
     *
     * @param bytes Destination byte array
     * @param num_bytes Number of bytes to store
     */
    void to_bytes(std::uint8_t* bytes, std::size_t num_bytes) const noexcept {
        std::size_t max_bytes = (num_bytes < NUM_BYTES) ? num_bytes : NUM_BYTES;
        std::size_t byte_idx = 0;

        for (std::size_t word_idx = 0; word_idx < NUM_WORDS && byte_idx < max_bytes; ++word_idx) {
            word_t word = data_[word_idx];

            for (int j = 3; j >= 0 && byte_idx < max_bytes; --j) {
                bytes[byte_idx] = static_cast<std::uint8_t>((word >> (j * 8)) & 0xFFU);
                ++byte_idx;
            }
        }
    }

    /**
     * @brief Get raw data pointer (for advanced use).
     * @return Pointer to word array
     */
    [[nodiscard]] word_t* data() noexcept {
        return data_.data();
    }

    /**
     * @brief Get raw data pointer (const version).
     * @return Pointer to word array
     */
    [[nodiscard]] const word_t* data() const noexcept {
        return data_.data();
    }

private:
    std::array<word_t, NUM_WORDS> data_;

    /**
     * @brief Mask off unused bits in the last word.
     */
    void mask_unused_bits() noexcept {
        constexpr std::size_t extra_bits = (NUM_WORDS * 32) - N;
        if constexpr (extra_bits > 0 && NUM_WORDS > 0) {
            word_t mask = ~((1U << extra_bits) - 1U);
            data_[NUM_WORDS - 1] &= mask;
        }
    }
};

} // namespace ccsds124

#endif // CCSDS124_BITVECTOR_HPP

/**
 * @file bitbuffer.hpp
 * @brief Variable-length bit buffer for building compressed output.
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
 * This module provides a dynamically-growing bit buffer for constructing
 * compressed output streams. Bits are appended sequentially using MSB-first
 * ordering as required by CCSDS 124.0-B-1.
 *
 * @par Bit Ordering
 * Bits are appended MSB-first within each byte:
 * - First bit appended goes to bit position 7
 * - Second bit goes to position 6, etc.
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef CCSDS124_BITBUFFER_HPP
#define CCSDS124_BITBUFFER_HPP

#include <array>
#include <cstring>

#include "bitvector.hpp"
#include "config.hpp"
#include "error.hpp"

namespace ccsds124 {

/**
 * @brief Variable-length bit buffer with static allocation.
 *
 * @tparam MaxBytes Maximum output size in bytes
 *
 * Uses static allocation only - no heap allocation. Suitable for
 * embedded systems with -fno-exceptions -fno-rtti.
 */
template <std::size_t MaxBytes = MAX_OUTPUT_BYTES> class BitBuffer {
public:
    /**
     * @brief Default constructor - initializes to empty state.
     */
    constexpr BitBuffer() noexcept : data_{}, num_bits_(0), acc_(0), acc_len_(0) {}

    /**
     * @brief Clear buffer to empty state.
     */
    void clear() noexcept {
        num_bits_ = 0;
        acc_ = 0;
        acc_len_ = 0;
        data_.fill(0);
    }

    /**
     * @brief Get number of bits in buffer.
     * @return Number of bits currently stored
     */
    [[nodiscard]] std::size_t size() const noexcept {
        return num_bits_;
    }

    /**
     * @brief Append a single bit.
     *
     * @param bit Bit value (0 or 1)
     * @return Error::Ok on success, Error::Overflow if buffer is full
     */
    Error append_bit(int bit) noexcept {
        constexpr std::size_t max_bits = MaxBytes * 8;
        if (num_bits_ >= max_bits) {
            return Error::Overflow;
        }

        // Accumulate bit in MSB-first order
        acc_ = (acc_ << 1) | (static_cast<std::uint32_t>(bit) & 1U);
        ++acc_len_;
        ++num_bits_;

        // Flush when accumulator has 8+ bits
        if (acc_len_ >= 8) {
            flush_acc();
        }

        return Error::Ok;
    }

    /**
     * @brief Append multiple bits from byte array.
     *
     * @param bytes Source bytes (MSB first within each byte)
     * @param num_bits Number of bits to append
     * @return Error::Ok on success
     */
    Error append_bits(const std::uint8_t* bytes, std::size_t num_bits) noexcept {
        constexpr std::size_t max_bits = MaxBytes * 8;
        if (num_bits_ + num_bits > max_bits) {
            return Error::Overflow;
        }

        // Process complete bytes efficiently
        std::size_t full_bytes = num_bits >> 3; // num_bits / 8
        for (std::size_t i = 0; i < full_bytes; ++i) {
            auto result = append_value(bytes[i], 8);
            if (result != Error::Ok) {
                return result;
            }
        }

        // Process remaining bits (0-7)
        std::size_t remaining_bits = num_bits & 7; // num_bits % 8
        if (remaining_bits > 0) {
            // Extract MSB-aligned bits from last byte
            std::uint32_t value = bytes[full_bytes] >> (8 - remaining_bits);
            auto result = append_value(value, remaining_bits);
            if (result != Error::Ok) {
                return result;
            }
        }

        return Error::Ok;
    }

    /**
     * @brief Append multiple bits from a value.
     *
     * @param value Value containing bits (right-justified)
     * @param num_bits Number of bits to append (1-24)
     * @return Error::Ok on success
     */
    Error append_value(std::uint32_t value, std::size_t num_bits) noexcept {
        if (num_bits == 0 || num_bits > 24) {
            return Error::InvalidArg;
        }

        constexpr std::size_t max_bits = MaxBytes * 8;
        if (num_bits_ + num_bits > max_bits) {
            return Error::Overflow;
        }

        // Mask to get only the relevant bits
        std::uint32_t mask = (1U << num_bits) - 1U;
        std::uint32_t masked_value = value & mask;

        acc_ = (acc_ << num_bits) | masked_value;
        acc_len_ += num_bits;
        num_bits_ += num_bits;

        // Flush complete bytes
        flush_acc();

        return Error::Ok;
    }

    /**
     * @brief Append bits from a BitVector.
     *
     * @tparam N BitVector length
     * @param bv Source bit vector
     * @param num_bits Number of bits to append
     * @return Error::Ok on success
     */
    template <std::size_t N>
    Error append_bitvector(const BitVector<N>& bv, std::size_t num_bits) noexcept {
        constexpr std::size_t max_bits = MaxBytes * 8;
        if (num_bits_ + num_bits > max_bits) {
            return Error::Overflow;
        }

        std::size_t bits_to_append = (num_bits < N) ? num_bits : N;
        std::size_t bit_pos = 0;

        // Process 24 bits at a time using direct word access
        while (bit_pos + 24 <= bits_to_append) {
            std::size_t word_idx = bit_pos >> 5;    // bit_pos / 32
            std::size_t bit_in_word = bit_pos & 31; // bit_pos % 32

            std::uint32_t value;
            if (bit_in_word <= 8) {
                // All 24 bits fit in current word (bit_in_word 0-8)
                std::uint32_t word = bv.data()[word_idx];
                value = (word >> (8 - bit_in_word)) & 0xFFFFFFU;
            } else if (bit_in_word + 24 <= 32) {
                // 24 bits fit in current word but not at convenient boundary
                std::uint32_t word = bv.data()[word_idx];
                value = (word >> (32 - bit_in_word - 24)) & 0xFFFFFFU;
            } else {
                // Span two words - extract bit-by-bit for correctness
                value = 0;
                for (std::size_t i = 0; i < 24; ++i) {
                    value = (value << 1) | static_cast<std::uint32_t>(bv.get_bit(bit_pos + i));
                }
            }

            auto result = append_value(value, 24);
            if (result != Error::Ok) {
                return result;
            }
            bit_pos += 24;
        }

        // Process remaining bits
        std::size_t remaining = bits_to_append - bit_pos;
        if (remaining > 0) {
            std::uint32_t value = 0;
            for (std::size_t i = 0; i < remaining; ++i) {
                value = (value << 1) | static_cast<std::uint32_t>(bv.get_bit(bit_pos + i));
            }
            auto result = append_value(value, remaining);
            if (result != Error::Ok) {
                return result;
            }
        }

        return Error::Ok;
    }

    /**
     * @brief Append all bits from a BitVector.
     *
     * @tparam N BitVector length
     * @param bv Source bit vector
     * @return Error::Ok on success
     */
    template <std::size_t N> Error append_bitvector(const BitVector<N>& bv) noexcept {
        return append_bitvector(bv, N);
    }

    /**
     * @brief Convert bit buffer to byte array.
     *
     * Pads final byte with zeros if not byte-aligned.
     *
     * @param bytes Destination byte array
     * @param max_bytes Maximum bytes to write
     * @return Number of bytes written
     */
    std::size_t to_bytes(std::uint8_t* bytes, std::size_t max_bytes) const noexcept {
        // Calculate number of bytes needed
        std::size_t num_bytes = (num_bits_ + 7) / 8;
        if (num_bytes > max_bytes) {
            num_bytes = max_bytes;
        }

        // Copy flushed bytes from data buffer
        std::size_t flushed_bytes = (num_bits_ - acc_len_) / 8;
        if (flushed_bytes > num_bytes) {
            flushed_bytes = num_bytes;
        }
        if (flushed_bytes > 0) {
            std::memcpy(bytes, data_.data(), flushed_bytes);
        }

        // Handle remaining bits in accumulator
        if (acc_len_ > 0 && flushed_bytes < num_bytes) {
            // Shift accumulator bits to MSB position
            bytes[flushed_bytes] = static_cast<std::uint8_t>(acc_ << (8 - acc_len_));
        }

        return num_bytes;
    }

private:
    std::array<std::uint8_t, MaxBytes> data_;
    std::size_t num_bits_;
    std::uint32_t acc_;
    std::size_t acc_len_;

    /**
     * @brief Flush accumulator to data buffer when it has 8+ bits.
     */
    void flush_acc() noexcept {
        while (acc_len_ >= 8) {
            // Extract top 8 bits
            acc_len_ -= 8;
            std::size_t byte_index = (num_bits_ - acc_len_ - 8) / 8;
            data_[byte_index] = static_cast<std::uint8_t>(acc_ >> acc_len_);
            // Only mask if there are remaining bits (avoid unnecessary op when acc_len_ == 0)
            if (acc_len_ > 0) {
                acc_ &= (1U << acc_len_) - 1U;
            }
        }
    }
};

} // namespace ccsds124

#endif // CCSDS124_BITBUFFER_HPP

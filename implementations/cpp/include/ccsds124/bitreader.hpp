/**
 * @file bitreader.hpp
 * @brief Sequential bit reading from compressed data.
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
 * The bit reader provides stateful bit-level access to compressed
 * packet data, reading MSB-first within each byte.
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef CCSDS124_BITREADER_HPP
#define CCSDS124_BITREADER_HPP

#include "config.hpp"

namespace ccsds124 {

/**
 * @brief Sequential bit reader for compressed data.
 *
 * Tracks position within a byte buffer for bit-level access.
 * Used during decompression to parse compressed packets.
 */
class BitReader {
public:
    /**
     * @brief Construct a bit reader.
     *
     * @param data Pointer to source data buffer
     * @param num_bits Number of valid bits in buffer
     */
    BitReader(const std::uint8_t* data, std::size_t num_bits) noexcept
        : data_(data), num_bits_(num_bits), bit_pos_(0) {}

    /**
     * @brief Read a single bit.
     *
     * @return Bit value (0 or 1), or -1 if no bits remaining
     */
    inline int read_bit() noexcept {
        if (bit_pos_ >= num_bits_) [[unlikely]] {
            return -1;
        }

        std::size_t byte_idx = bit_pos_ >> 3; // bit_pos_ / 8
        std::size_t bit_idx = bit_pos_ & 7;   // bit_pos_ % 8

        // MSB-first: bit 0 of byte is at position 7
        int bit = (data_[byte_idx] >> (7 - bit_idx)) & 1;
        ++bit_pos_;

        return bit;
    }

    /**
     * @brief Read multiple bits as unsigned value.
     *
     * Reads up to 32 bits MSB-first and returns as uint32_t.
     * Optimized with fast path for byte-aligned reads.
     *
     * @param num_bits Number of bits to read (1-32)
     * @return Unsigned value from bits read
     */
    std::uint32_t read_bits(std::size_t num_bits) noexcept {
        if (num_bits == 0 || num_bits > 32) [[unlikely]] {
            return 0;
        }

        if (bit_pos_ + num_bits > num_bits_) [[unlikely]] {
            return 0; // Underflow protection
        }

        // Fast path: byte-aligned reads (common case)
        if ((bit_pos_ & 7) == 0 && (num_bits & 7) == 0) [[likely]] {
            std::size_t byte_idx = bit_pos_ >> 3;
            std::size_t num_bytes = num_bits >> 3;
            std::uint32_t value = 0;
            for (std::size_t i = 0; i < num_bytes; ++i) {
                value = (value << 8) | data_[byte_idx + i];
            }
            bit_pos_ += num_bits;
            return value;
        }

        // General case: unaligned reads
        std::uint32_t value = 0;
        std::size_t remaining = num_bits;

        while (remaining > 0) {
            std::size_t byte_idx = bit_pos_ >> 3;
            std::size_t bit_idx = bit_pos_ & 7;

            // Bits available in current byte
            std::size_t bits_in_byte = 8 - bit_idx;
            std::size_t bits_to_read = (remaining < bits_in_byte) ? remaining : bits_in_byte;

            // Extract bits from current byte (MSB-first)
            std::uint8_t byte_val = data_[byte_idx];
            std::uint32_t shift = 8 - bit_idx - bits_to_read;
            std::uint32_t mask = (1U << bits_to_read) - 1U;
            std::uint32_t extracted = (byte_val >> shift) & mask;

            value = (value << bits_to_read) | extracted;
            bit_pos_ += bits_to_read;
            remaining -= bits_to_read;
        }

        return value;
    }

    /**
     * @brief Get current bit position.
     *
     * @return Number of bits already read
     */
    [[nodiscard]] std::size_t position() const noexcept {
        return bit_pos_;
    }

    /**
     * @brief Get remaining bits.
     *
     * @return Number of bits remaining to read
     */
    [[nodiscard]] std::size_t remaining() const noexcept {
        return (bit_pos_ < num_bits_) ? (num_bits_ - bit_pos_) : 0;
    }

    /**
     * @brief Skip to next byte boundary.
     *
     * Advances position to start of next byte (for padding).
     */
    void align_byte() noexcept {
        std::size_t bit_offset = bit_pos_ & 7; // bit_pos_ % 8
        if (bit_offset != 0) {
            bit_pos_ += (8 - bit_offset);
        }
    }

private:
    const std::uint8_t* data_;
    std::size_t num_bits_;
    std::size_t bit_pos_;
};

} // namespace ccsds124

#endif // CCSDS124_BITREADER_HPP

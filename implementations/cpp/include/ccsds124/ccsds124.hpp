/**
 * @file ccsds124.hpp
 * @brief High-level CCSDS 124.0-B-1 compression API.
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
 * Provides high-level compress() and decompress() functions for bulk data
 * processing, suitable for file-level operations.
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef CCSDS124_HPP
#define CCSDS124_HPP

#include <cstring>

#include "bitbuffer.hpp"
#include "bitreader.hpp"
#include "bitvector.hpp"
#include "compressor.hpp"
#include "config.hpp"
#include "decompressor.hpp"
#include "error.hpp"

namespace ccsds124 {

/**
 * @brief Compress multi-packet data stream.
 *
 * Compresses input data as a sequence of fixed-size packets using
 * CCSDS 124.0-B-1 algorithm.
 *
 * @tparam N Packet length in bits
 * @param input_data Input data bytes
 * @param input_size Input size in bytes (must be multiple of packet size)
 * @param output_buffer Output buffer for compressed data
 * @param output_buffer_size Maximum output buffer size
 * @param[out] output_size Actual compressed output size
 * @param robustness Robustness level R (0-7)
 * @param pt_limit New mask period (pt parameter)
 * @param ft_limit Send mask period (ft parameter)
 * @param rt_limit Uncompressed period (rt parameter)
 * @return Error::Ok on success
 */
template <std::size_t N>
Error compress(const std::uint8_t* input_data, std::size_t input_size, std::uint8_t* output_buffer,
               std::size_t output_buffer_size, std::size_t& output_size,
               std::uint8_t robustness = 0, int pt_limit = 0, int ft_limit = 0,
               int rt_limit = 0) noexcept {
    // Calculate packet size in bytes
    constexpr std::size_t packet_bytes = (N + 7) / 8;

    // Verify input is multiple of packet size
    if (input_size == 0 || (input_size % packet_bytes) != 0) {
        return Error::InvalidArg;
    }

    // Calculate number of packets
    std::size_t num_packets = input_size / packet_bytes;

    // Initialize compressor
    Compressor<N> comp(robustness, pt_limit, ft_limit, rt_limit);

    // Track output position
    std::size_t total_output = 0;

    // Countdown counters for automatic parameter management
    int pt_counter = pt_limit;
    int ft_counter = ft_limit;
    int rt_counter = rt_limit;

    // Process each packet
    for (std::size_t i = 0; i < num_packets; ++i) {
        // Load packet data
        BitVector<N> input_vec;
        input_vec.from_bytes(&input_data[i * packet_bytes], packet_bytes);

        // Compute parameters
        CompressParams params;
        params.min_robustness = robustness;

        if (pt_limit > 0 && ft_limit > 0 && rt_limit > 0) {
            if (i == 0) {
                // First packet: fixed init values
                params.send_mask_flag = true;
                params.uncompressed_flag = true;
                params.new_mask_flag = false;
            } else {
                // ft counter
                if (ft_counter == 1) {
                    params.send_mask_flag = true;
                    ft_counter = ft_limit;
                } else {
                    --ft_counter;
                    params.send_mask_flag = false;
                }

                // pt counter
                if (pt_counter == 1) {
                    params.new_mask_flag = true;
                    pt_counter = pt_limit;
                } else {
                    --pt_counter;
                    params.new_mask_flag = false;
                }

                // rt counter
                if (rt_counter == 1) {
                    params.uncompressed_flag = true;
                    rt_counter = rt_limit;
                } else {
                    --rt_counter;
                    params.uncompressed_flag = false;
                }

                // Override for init packets (CCSDS requirement)
                if (i <= static_cast<std::size_t>(robustness)) {
                    params.send_mask_flag = true;
                    params.uncompressed_flag = true;
                    params.new_mask_flag = false;
                }
            }
        } else {
            // Manual control: defaults
            params.send_mask_flag = false;
            params.uncompressed_flag = false;
            params.new_mask_flag = false;
        }

        // Compress packet
        BitBuffer<N * 6> packet_output;
        auto result = comp.compress_packet(input_vec, packet_output, &params);
        if (result != Error::Ok) {
            return result;
        }

        // Convert to bytes with padding
        std::uint8_t packet_bytes_out[N * 6 / 8 + 1];
        std::size_t packet_size =
            packet_output.to_bytes(packet_bytes_out, sizeof(packet_bytes_out));

        // Check buffer space
        if (total_output + packet_size > output_buffer_size) {
            return Error::Overflow;
        }

        // Copy to output
        std::memcpy(&output_buffer[total_output], packet_bytes_out, packet_size);
        total_output += packet_size;
    }

    output_size = total_output;
    return Error::Ok;
}

/**
 * @brief Decompress multi-packet data stream.
 *
 * Decompresses CCSDS 124.0-B-1 compressed data back to original packets.
 *
 * @tparam N Packet length in bits
 * @param input_data Compressed input data bytes
 * @param input_size Compressed input size in bytes
 * @param output_buffer Output buffer for decompressed data
 * @param output_buffer_size Maximum output buffer size
 * @param[out] output_size Actual decompressed output size
 * @param robustness Robustness level R (0-7)
 * @return Error::Ok on success
 */
template <std::size_t N>
Error decompress(const std::uint8_t* input_data, std::size_t input_size,
                 std::uint8_t* output_buffer, std::size_t output_buffer_size,
                 std::size_t& output_size, std::uint8_t robustness = 0) noexcept {
    // Calculate packet size in bytes
    constexpr std::size_t packet_bytes = (N + 7) / 8;

    // Initialize decompressor
    Decompressor<N> decomp(robustness);

    // Initialize bit reader
    BitReader reader(input_data, input_size * 8);

    // Track output position
    std::size_t total_output = 0;

    // Decompress packets until input exhausted
    while (reader.remaining() > 0) {
        BitVector<N> output;
        auto result = decomp.decompress_packet(reader, output);
        if (result != Error::Ok) {
            return result;
        }

        // Check output buffer space
        if (total_output + packet_bytes > output_buffer_size) {
            return Error::Overflow;
        }

        // Copy to output buffer
        output.to_bytes(&output_buffer[total_output], packet_bytes);
        total_output += packet_bytes;

        // Align to byte boundary for next packet
        reader.align_byte();
    }

    output_size = total_output;
    return Error::Ok;
}

/**
 * @brief Get library version.
 * @return Version string
 */
inline const char* version() noexcept {
    return "1.0.0";
}

} // namespace ccsds124

#endif // CCSDS124_HPP

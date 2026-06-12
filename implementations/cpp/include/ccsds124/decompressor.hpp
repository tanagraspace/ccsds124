/**
 * @file decompressor.hpp
 * @brief CCSDS 124.0-B-1 decompression algorithm implementation.
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
 * - Packet decompression and mask reconstruction
 * - Full round-trip support with Compressor
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef CCSDS124_DECOMPRESSOR_HPP
#define CCSDS124_DECOMPRESSOR_HPP

#include "bitreader.hpp"
#include "bitvector.hpp"
#include "config.hpp"
#include "decoder.hpp"
#include "error.hpp"

namespace ccsds124 {

/**
 * @brief CCSDS 124.0-B-1 decompressor with static memory allocation.
 *
 * Template class that implements the CCSDS 124.0-B-1 decompression algorithm.
 * All memory is statically allocated based on template parameters.
 *
 * @tparam N Packet length in bits (F)
 */
template <std::size_t N> class Decompressor {
public:
    /**
     * @brief Construct decompressor with configuration.
     *
     * @param robustness Base robustness level R_t (0-7)
     */
    explicit Decompressor(std::uint8_t robustness = 0) noexcept
        : robustness_(robustness > MAX_ROBUSTNESS ? static_cast<std::uint8_t>(MAX_ROBUSTNESS)
                                                  : robustness) {
        reset();
    }

    /**
     * @brief Set the initial mask.
     *
     * @param initial_mask Initial mask M_0
     */
    void set_initial_mask(const BitVector<N>& initial_mask) noexcept {
        initial_mask_ = initial_mask;
        mask_ = initial_mask;
    }

    /**
     * @brief Reset decompressor to initial state.
     */
    void reset() noexcept {
        t_ = 0;
        mask_ = initial_mask_;
        prev_output_.zero();
        Xt_.zero();
    }

    /**
     * @brief Decompress a single compressed packet.
     *
     * @param reader Bit reader positioned at packet start
     * @param output Decompressed output packet (length N)
     * @return Error::Ok on success
     */
    Error decompress_packet(BitReader& reader, BitVector<N>& output) noexcept {
        // Copy previous output as prediction base
        output = prev_output_;

        // Clear positive changes tracker
        Xt_.zero();

        // ====================================================================
        // Parse h_t: Mask change information
        // h_t = RLE(X_t) || BIT_4(V_t) || e_t || k_t || c_t || d_t
        // ====================================================================

        // Decode RLE(X_t) - mask changes
        BitVector<N> Xt;
        auto status = rle_decode(reader, Xt);
        if (status != Error::Ok) {
            return status;
        }

        // Read BIT_4(V_t) - effective robustness
        if (reader.remaining() < 4) {
            return Error::Underflow;
        }
        std::uint32_t vt_raw = reader.read_bits(4);
        std::uint8_t Vt = static_cast<std::uint8_t>(vt_raw & 0x0FU);

        // Process e_t, k_t, c_t if V_t > 0 and there are changes
        int ct = 0;
        std::size_t change_count = Xt.hamming_weight();

        if (Vt > 0 && change_count > 0) {
            // Read e_t
            int et = reader.read_bit();
            if (et < 0) {
                return Error::Underflow;
            }

            if (et == 1) {
                // Read k_t bits and apply mask updates in single pass
                // Iterate by set bits in Xt using word-by-word traversal
                for (std::size_t word = 0; word < BitVector<N>::NUM_WORDS; ++word) {
                    std::uint32_t xt_word = Xt.data()[word];
                    // Loop bounded by popcount - guarantees termination
                    for (int bits_remaining = __builtin_popcount(xt_word); bits_remaining > 0;
                         --bits_remaining) {
                        // Find MSB position (forward order)
                        int clz = detail::extract_msb(xt_word);
                        std::size_t global_pos = (word * 32) + static_cast<std::size_t>(clz);

                        if (global_pos < N) {
                            int kt_bit = reader.read_bit();
                            if (kt_bit < 0) {
                                return Error::Underflow;
                            }
                            // kt=1 means positive update (mask becomes 0)
                            // kt=0 means negative update (mask becomes 1)
                            if (kt_bit > 0) {
                                mask_.set_bit_unchecked(global_pos, 0);
                                Xt_.set_bit_unchecked(global_pos, 1); // Track positive change
                            } else {
                                mask_.set_bit_unchecked(global_pos, 1);
                            }
                        }
                    }
                }

                // Read c_t
                ct = reader.read_bit();
                if (ct < 0) {
                    return Error::Underflow;
                }
            } else {
                // et = 0: all updates are negative (mask bits become 1)
                // Iterate by set bits in Xt
                for (std::size_t word = 0; word < BitVector<N>::NUM_WORDS; ++word) {
                    std::uint32_t xt_word = Xt.data()[word];
                    // Loop bounded by popcount - guarantees termination
                    for (int bits_remaining = __builtin_popcount(xt_word); bits_remaining > 0;
                         --bits_remaining) {
                        int clz = detail::extract_msb(xt_word);
                        std::size_t global_pos = (word * 32) + static_cast<std::size_t>(clz);
                        if (global_pos < N) {
                            mask_.set_bit_unchecked(global_pos, 1);
                        }
                    }
                }
            }
        } else if (Vt == 0 && change_count > 0) {
            // Vt = 0: toggle mask bits at change positions
            // Iterate by set bits in Xt
            for (std::size_t word = 0; word < BitVector<N>::NUM_WORDS; ++word) {
                std::uint32_t xt_word = Xt.data()[word];
                // Loop bounded by popcount - guarantees termination
                for (int bits_remaining = __builtin_popcount(xt_word); bits_remaining > 0;
                     --bits_remaining) {
                    int clz = detail::extract_msb(xt_word);
                    std::size_t global_pos = (word * 32) + static_cast<std::size_t>(clz);
                    if (global_pos < N) {
                        int current_val = mask_.get_bit_unchecked(global_pos);
                        mask_.set_bit_unchecked(global_pos, current_val == 0 ? 1 : 0);
                    }
                }
            }
        }
        // else: No changes to apply (change_count == 0)

        // Read d_t
        int dt = reader.read_bit();
        if (dt < 0) {
            return Error::Underflow;
        }

        // ====================================================================
        // Parse q_t: Optional full mask
        // ====================================================================

        int rt = 0;

        // dt=1 means both ft=0 and rt=0 (optimization per CCSDS Eq. 13)
        // dt=0 means we need to read ft and rt from the stream
        if (dt == 0) {
            // Read ft flag
            int ft = reader.read_bit();
            if (ft < 0) {
                return Error::Underflow;
            }

            if (ft == 1) {
                // Full mask follows: decode RLE(M XOR (M<<))
                BitVector<N> mask_diff;
                status = rle_decode(reader, mask_diff);
                if (status != Error::Ok) {
                    return status;
                }

                // Reverse the horizontal XOR to get the actual mask.
                // HXOR encoding: HXOR[i] = M[i] XOR M[i+1], with HXOR[F-1] = M[F-1]
                // Reversal: start from LSB (position F-1) and work towards MSB (position 0)
                // M[F-1] = HXOR[F-1] (just copy)
                // M[i] = HXOR[i] XOR M[i+1] for i < F-1

                // Copy LSB bit directly (position F-1 in bitvector)
                int current = mask_diff.get_bit(N - 1);
                mask_.set_bit(N - 1, current);

                // Process remaining bits from F-2 down to 0
                for (std::size_t i = N - 1; i > 0; --i) {
                    std::size_t pos = i - 1;
                    int hxor_bit = mask_diff.get_bit(pos);
                    // M[pos] = HXOR[pos] XOR M[pos+1] = HXOR[pos] XOR current
                    current = hxor_bit ^ current;
                    mask_.set_bit(pos, current);
                }
            }

            // Read rt flag
            rt = reader.read_bit();
            if (rt < 0) {
                return Error::Underflow;
            }
        }

        if (rt == 1) {
            // Full packet follows: COUNT(F) || I_t
            std::uint32_t packet_length = 0;
            status = count_decode(reader, packet_length);
            if (status != Error::Ok) {
                return status;
            }

            // Read full packet
            for (std::size_t i = 0; i < N; ++i) {
                int bit = reader.read_bit();
                if (bit < 0) {
                    return Error::Underflow;
                }
                output.set_bit(i, bit > 0 ? 1 : 0);
            }
        } else {
            // Compressed: extract unpredictable bits
            BitVector<N> extraction_mask;

            if (ct == 1 && Vt > 0) {
                // BE(I_t, (X_t OR M_t))
                extraction_mask.or_of(mask_, Xt_);
            } else {
                // BE(I_t, M_t)
                extraction_mask = mask_;
            }

            // Insert unpredictable bits
            status = bit_insert(reader, output, extraction_mask);
            if (status != Error::Ok) {
                return status;
            }
        }

        // ====================================================================
        // Update state for next cycle
        // ====================================================================

        prev_output_ = output;
        ++t_;

        return Error::Ok;
    }

    /**
     * @brief Get current time index.
     */
    std::size_t time_index() const noexcept {
        return t_;
    }

    /**
     * @brief Get robustness level.
     */
    std::uint8_t robustness() const noexcept {
        return robustness_;
    }

    /**
     * @brief Get current mask.
     */
    const BitVector<N>& mask() const noexcept {
        return mask_;
    }

private:
    // Configuration
    std::uint8_t robustness_;

    // State
    BitVector<N> initial_mask_;
    BitVector<N> mask_;
    BitVector<N> prev_output_;
    BitVector<N> Xt_; // Positive changes tracker

    // Cycle counter
    std::size_t t_ = 0;
};

} // namespace ccsds124

#endif // CCSDS124_DECOMPRESSOR_HPP

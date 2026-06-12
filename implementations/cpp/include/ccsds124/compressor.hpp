/**
 * @file compressor.hpp
 * @brief CCSDS 124.0-B-1 compression algorithm implementation.
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
 * Implements CCSDS 124.0-B-1 Section 5.3 (Encoding Step):
 * - Compressor initialization and state management
 * - Main compression algorithm
 * - Output packet encoding: o_t = h_t || q_t || u_t
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef CCSDS124_COMPRESSOR_HPP
#define CCSDS124_COMPRESSOR_HPP

#include "bitbuffer.hpp"
#include "bitvector.hpp"
#include "config.hpp"
#include "encoder.hpp"
#include "error.hpp"
#include "mask.hpp"

namespace ccsds124 {

/**
 * @brief Compression parameters for a single packet.
 */
struct CompressParams {
    std::uint8_t min_robustness = 0; ///< R_t: Minimum robustness level (0-7)
    bool new_mask_flag = false;      ///< p_t: Update mask from build vector
    bool send_mask_flag = false;     ///< f_t: Include mask in output
    bool uncompressed_flag = false;  ///< r_t: Send uncompressed
};

/**
 * @brief CCSDS 124.0-B-1 compressor with static memory allocation.
 *
 * Template class that implements the CCSDS 124.0-B-1 compression algorithm.
 * All memory is statically allocated based on template parameters.
 *
 * @tparam N Packet length in bits (F)
 * @tparam MaxOutputBytes Maximum output buffer size
 */
template <std::size_t N, std::size_t MaxOutputBytes = N * 6> class Compressor {
public:
    /**
     * @brief Construct compressor with configuration.
     *
     * @param robustness Base robustness level R_t (0-7)
     * @param pt_limit New mask period (0 = manual control)
     * @param ft_limit Send mask period (0 = manual control)
     * @param rt_limit Uncompressed period (0 = manual control)
     */
    explicit Compressor(std::uint8_t robustness = 0, int pt_limit = 0, int ft_limit = 0,
                        int rt_limit = 0) noexcept
        : robustness_(robustness > MAX_ROBUSTNESS ? MAX_ROBUSTNESS : robustness),
          pt_limit_(pt_limit),
          ft_limit_(ft_limit),
          rt_limit_(rt_limit),
          pt_counter_(pt_limit),
          ft_counter_(ft_limit),
          rt_counter_(rt_limit) {
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
     * @brief Reset compressor to initial state.
     *
     * Resets time index to 0 and clears all history while
     * preserving configuration.
     */
    void reset() noexcept {
        t_ = 0;
        history_index_ = 0;
        flag_history_index_ = 0;

        // Reset mask to initial
        mask_ = initial_mask_;
        prev_mask_.zero();
        build_.zero();
        prev_input_.zero();

        // Clear change history and weight cache
        for (std::size_t i = 0; i < MAX_HISTORY; ++i) {
            change_history_[i].zero();
            change_weight_history_[i] = 0;
        }

        // Clear flag history
        for (std::size_t i = 0; i < MAX_VT_HISTORY; ++i) {
            new_mask_flag_history_[i] = 0;
        }

        // Reset countdown counters
        pt_counter_ = pt_limit_;
        ft_counter_ = ft_limit_;
        rt_counter_ = rt_limit_;
    }

    /**
     * @brief Compress a single input packet.
     *
     * @tparam OutputSize Output buffer size in bytes
     * @param input Input packet I_t (must be length N)
     * @param output Compressed output buffer
     * @param params Compression parameters (nullptr = use automatic)
     * @return Error::Ok on success
     */
    template <std::size_t OutputSize>
    Error compress_packet(const BitVector<N>& input, BitBuffer<OutputSize>& output,
                          const CompressParams* params = nullptr) noexcept {
        // Get parameters (use defaults if nullptr)
        CompressParams effective_params;
        if (params != nullptr) {
            effective_params = *params;
        } else {
            effective_params.min_robustness = robustness_;
        }

        // Clear output buffer
        output.clear();

        // ====================================================================
        // STEP 1: Update Mask and Build Vectors (CCSDS Section 4)
        // ====================================================================

        // Save previous mask
        prev_mask_ = mask_;

        // Update build and mask vectors (only when t > 0)
        if (t_ > 0) {
            // Save previous build (only needed when updating)
            work_prev_build_ = build_;

            // Update build vector (Equation 6)
            update_build(build_, input, prev_input_, effective_params.new_mask_flag, t_);

            // Update mask vector (Equation 7)
            update_mask(mask_, input, prev_input_, work_prev_build_,
                        effective_params.new_mask_flag);
        }

        // Compute change vector (Equation 8)
        compute_change(work_change_, mask_, prev_mask_, t_);

        // Store change in history (circular buffer) with cached weight
        change_history_[history_index_] = work_change_;
        change_weight_history_[history_index_] = work_change_.hamming_weight();

        // ====================================================================
        // STEP 2: Encode Output Packet (CCSDS Section 5.3)
        // o_t = h_t || q_t || u_t
        // ====================================================================

        // Calculate X_t (robustness window)
        compute_robustness_window(work_Xt_, work_change_);

        // Calculate V_t (effective robustness)
        std::uint8_t Vt = compute_effective_robustness();

        // Calculate c_t flag (cached - used twice below)
        bool ct = compute_ct_flag(Vt, effective_params.new_mask_flag);

        // Calculate d_t flag
        bool dt = !effective_params.send_mask_flag && !effective_params.uncompressed_flag;

        // ====================================================================
        // Component h_t: Mask change information
        // h_t = RLE(X_t) || BIT_4(V_t) || e_t || k_t || c_t || d_t
        // ====================================================================

        // 1. RLE(X_t) - Run-length encode the robustness window
        auto rle_result = rle_encode(output, work_Xt_);
        if (rle_result != Error::Ok)
            return rle_result;

        // 2. BIT_4(V_t) - 4-bit effective robustness level (optimized: single append)
        auto vt_result = output.append_value(Vt & 0x0FU, 4);
        if (vt_result != Error::Ok)
            return vt_result;

        // Cache hamming weight (avoid recomputing)
        std::size_t xt_weight = work_Xt_.hamming_weight();

        // 3. e_t, k_t, c_t - Only if V_t > 0 and there are mask changes
        if (Vt > 0 && xt_weight > 0) {
            // Calculate e_t
            bool et = has_positive_updates(work_Xt_, mask_);

            auto result = output.append_bit(et ? 1 : 0);
            if (result != Error::Ok)
                return result;

            if (et) {
                // k_t - Output '1' for positive updates (mask=0), '0' for negative (mask=1)
                // Extract INVERTED mask values
                work_inverted_.not_of(mask_);

                auto kt_result = bit_extract_forward(output, work_inverted_, work_Xt_);
                if (kt_result != Error::Ok)
                    return kt_result;

                // Encode c_t (already computed above)
                result = output.append_bit(ct ? 1 : 0);
                if (result != Error::Ok)
                    return result;
            }
        }

        // 4. d_t - Flag indicating if both f_t and r_t are zero
        auto dt_result = output.append_bit(dt ? 1 : 0);
        if (dt_result != Error::Ok)
            return dt_result;

        // ====================================================================
        // Component q_t: Optional full mask
        // q_t = empty if d_t=1, '1' || RLE(<(M_t XOR (M_t<<))>) if f_t=1, '0' otherwise
        // ====================================================================

        if (!dt) { // Only if d_t = 0
            if (effective_params.send_mask_flag) {
                auto result = output.append_bit(1); // Flag: mask follows
                if (result != Error::Ok)
                    return result;

                // Encode mask as RLE(M XOR (M<<))
                work_shifted_.left_shift(mask_);
                work_diff_.xor_with(mask_, work_shifted_);

                auto rle_mask_result = rle_encode(output, work_diff_);
                if (rle_mask_result != Error::Ok)
                    return rle_mask_result;
            } else {
                auto result = output.append_bit(0); // Flag: no mask
                if (result != Error::Ok)
                    return result;
            }
        }

        // ====================================================================
        // Component u_t: Unpredictable bits or full input
        // ====================================================================

        if (effective_params.uncompressed_flag) {
            // '1' || COUNT(F) || I_t
            auto result = output.append_bit(1); // Flag: full input follows
            if (result != Error::Ok)
                return result;

            auto count_result = count_encode(output, static_cast<std::uint32_t>(N));
            if (count_result != Error::Ok)
                return count_result;

            auto append_result = output.append_bitvector(input);
            if (append_result != Error::Ok)
                return append_result;
        } else {
            if (!dt) {
                // '0' || BE(...)
                auto result = output.append_bit(0); // Flag: compressed
                if (result != Error::Ok)
                    return result;
            }

            // Use extraction mask based on c_t (already computed above)
            if (ct && Vt > 0) {
                // BE(I_t, (X_t OR M_t)) - extract bits where mask OR changes are set
                work_diff_.or_with(mask_, work_Xt_); // M_t OR X_t
                auto be_result = bit_extract(output, input, work_diff_);
                if (be_result != Error::Ok)
                    return be_result;
            } else {
                // BE(I_t, M_t) - extract only unpredictable bits
                auto be_result = bit_extract(output, input, mask_);
                if (be_result != Error::Ok)
                    return be_result;
            }
        }

        // ====================================================================
        // STEP 3: Update State for Next Cycle
        // ====================================================================

        // Save current input for next iteration
        prev_input_ = input;

        // Track new_mask_flag for c_t calculation
        new_mask_flag_history_[flag_history_index_] = effective_params.new_mask_flag ? 1 : 0;
        flag_history_index_ = (flag_history_index_ + 1) & (MAX_VT_HISTORY - 1);

        // Advance time
        ++t_;

        // Advance history index (circular buffer)
        history_index_ = (history_index_ + 1) & (MAX_HISTORY - 1);

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

    /**
     * @brief Compute parameters for automatic mode.
     *
     * @param packet_index Current packet index (0-based)
     * @return Computed parameters
     */
    CompressParams compute_auto_params(std::size_t packet_index) noexcept {
        CompressParams params;
        params.min_robustness = robustness_;

        if (pt_limit_ > 0 && ft_limit_ > 0 && rt_limit_ > 0) {
            if (packet_index == 0) {
                // First packet: fixed init values
                params.send_mask_flag = true;
                params.uncompressed_flag = true;
                params.new_mask_flag = false;
            } else {
                // Packets 1+: check and update countdown counters
                // ft counter
                if (ft_counter_ == 1) {
                    params.send_mask_flag = true;
                    ft_counter_ = ft_limit_;
                } else {
                    --ft_counter_;
                    params.send_mask_flag = false;
                }

                // pt counter
                if (pt_counter_ == 1) {
                    params.new_mask_flag = true;
                    pt_counter_ = pt_limit_;
                } else {
                    --pt_counter_;
                    params.new_mask_flag = false;
                }

                // rt counter
                if (rt_counter_ == 1) {
                    params.uncompressed_flag = true;
                    rt_counter_ = rt_limit_;
                } else {
                    --rt_counter_;
                    params.uncompressed_flag = false;
                }

                // Override for remaining init packets: CCSDS requires first R_t+1 packets
                // to have f_t=1, r_t=1, p_t=0
                if (packet_index <= static_cast<std::size_t>(robustness_)) {
                    params.send_mask_flag = true;
                    params.uncompressed_flag = true;
                    params.new_mask_flag = false;
                }
            }
        }

        return params;
    }

private:
    /**
     * @brief Compute robustness window X_t.
     *
     * X_t = <(D_{t-R_t} OR D_{t-R_t+1} OR ... OR D_t)>
     */
    void compute_robustness_window(BitVector<N>& Xt, const BitVector<N>& current_change) noexcept {
        if (robustness_ == 0 || t_ == 0) {
            // X_t = D_t
            Xt = current_change;
        } else {
            // Start with current change
            Xt = current_change;

            // Determine how many historical changes to include
            std::size_t num_changes = (t_ < robustness_) ? t_ : robustness_;
            constexpr std::size_t history_mask = MAX_HISTORY - 1; // For bitwise AND

            // OR with historical changes (going backwards from current)
            for (std::size_t i = 1; i <= num_changes; ++i) {
                std::size_t hist_idx = (history_index_ + MAX_HISTORY - i) & history_mask;
                Xt.or_in_place(change_history_[hist_idx]);
            }
        }
    }

    /**
     * @brief Compute effective robustness V_t.
     *
     * V_t = R_t + C_t where C_t = consecutive iterations without mask changes
     */
    std::uint8_t compute_effective_robustness() const noexcept {
        // Early exit for common case: V_t = R_t when t <= R_t
        if (t_ <= robustness_) [[likely]] {
            return robustness_;
        }

        // For t > R_t, compute C_t (consecutive iterations without changes)
        std::uint8_t Vt = robustness_;
        constexpr std::uint8_t max_Vt = 15;
        constexpr std::size_t history_mask = MAX_HISTORY - 1; // For bitwise AND (16 -> 15)
        const std::size_t max_check = (t_ < 15) ? t_ : 15;

        for (std::size_t i = robustness_ + 1; i <= max_check; ++i) {
            std::size_t hist_idx = (history_index_ + MAX_HISTORY - i) & history_mask;
            // Use cached weight instead of computing hamming_weight()
            if (change_weight_history_[hist_idx] > 0) {
                break; // Found a change, stop counting
            }
            ++Vt;
            if (Vt >= max_Vt) {
                break; // Cap at 15
            }
        }

        return Vt;
    }

    /**
     * @brief Check for positive mask updates (e_t flag).
     *
     * e_t = 1 if any changed bits (in X_t) are predictable (mask bit = 0)
     */
    static bool has_positive_updates(const BitVector<N>& Xt, const BitVector<N>& mask) noexcept {
        // e_t = 1 if (Xt AND (NOT mask)) != 0
        for (std::size_t w = 0; w < BitVector<N>::NUM_WORDS; ++w) {
            if ((Xt.data()[w] & ~mask.data()[w]) != 0) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Compute c_t flag for multiple mask updates.
     *
     * c_t = 1 if new_mask_flag was set 2+ times in last V_t+1 iterations
     */
    bool compute_ct_flag(std::uint8_t Vt, bool current_new_mask_flag) const noexcept {
        if (Vt == 0) {
            return false;
        }

        int count = 0;

        // Include current packet's flag
        if (current_new_mask_flag) {
            ++count;
        }

        // Check history for V_t previous entries
        std::size_t iterations_to_check = (Vt <= t_) ? Vt : t_;
        constexpr std::size_t vt_history_mask = MAX_VT_HISTORY - 1; // For bitwise AND

        for (std::size_t i = 0; i < iterations_to_check; ++i) {
            std::size_t hist_idx = (flag_history_index_ + MAX_VT_HISTORY - 1 - i) & vt_history_mask;
            if (new_mask_flag_history_[hist_idx] != 0) {
                ++count;
            }
        }

        return count >= 2;
    }

    // Configuration (immutable after construction)
    std::uint8_t robustness_;
    int pt_limit_;
    int ft_limit_;
    int rt_limit_;

    // State (updated each cycle)
    BitVector<N> initial_mask_;
    BitVector<N> mask_;
    BitVector<N> prev_mask_;
    BitVector<N> build_;
    BitVector<N> prev_input_;
    BitVector<N> change_history_[MAX_HISTORY];
    std::size_t history_index_ = 0;

    // History for c_t calculation
    std::uint8_t new_mask_flag_history_[MAX_VT_HISTORY] = {0};
    std::size_t flag_history_index_ = 0;

    // Cached hamming weights for change history (avoids recomputing in
    // compute_effective_robustness)
    std::size_t change_weight_history_[MAX_HISTORY] = {0};

    // Cycle counter
    std::size_t t_ = 0;

    // Parameter management (automatic mode)
    int pt_counter_;
    int ft_counter_;
    int rt_counter_;

    // Pre-allocated work buffers
    BitVector<N> work_prev_build_;
    BitVector<N> work_change_;
    BitVector<N> work_Xt_;
    BitVector<N> work_inverted_;
    BitVector<N> work_shifted_;
    BitVector<N> work_diff_;
};

} // namespace ccsds124

#endif // CCSDS124_COMPRESSOR_HPP

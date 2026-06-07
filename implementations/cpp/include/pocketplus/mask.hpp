/**
 * @file mask.hpp
 * @brief POCKET+ mask update logic.
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
 * Implements CCSDS 124.0-B-1 Section 4 (Mask Update):
 * - Build Vector Update (Equation 6)
 * - Mask Vector Update (Equation 7)
 * - Change Vector Computation (Equation 8)
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef POCKETPLUS_MASK_HPP
#define POCKETPLUS_MASK_HPP

#include "bitvector.hpp"
#include "config.hpp"
#include "error.hpp"

namespace pocketplus {

/**
 * @brief Update build vector (CCSDS Equation 6).
 *
 * The build vector accumulates XOR differences between consecutive
 * packets to track which bits have changed.
 *
 * - B_t = (I_t XOR I_{t-1}) OR B_{t-1} (if t > 0 and new_mask_flag = 0)
 * - B_t = 0 (otherwise: t=0 or new_mask_flag=1)
 *
 * @tparam N BitVector length
 * @param[in,out] build Updated build vector B_t
 * @param[in] input Current input I_t
 * @param[in] prev_input Previous input I_{t-1}
 * @param[in] new_mask_flag p_t flag (true = reset build)
 * @param[in] t Time index (0 = first packet)
 */
template <std::size_t N>
void update_build(BitVector<N>& build, const BitVector<N>& input, const BitVector<N>& prev_input,
                  bool new_mask_flag, std::size_t t) noexcept {
    if (t == 0 || new_mask_flag) {
        // Case 1: t=0 or new_mask_flag set -> reset build to 0
        build.zero();
    } else {
        // Case 2: Normal operation (t > 0 and new_mask_flag = 0)
        // B_t = (I_t XOR I_{t-1}) OR B_{t-1}
        // Optimized: compute in-place without temporary allocation
        for (std::size_t w = 0; w < BitVector<N>::NUM_WORDS; ++w) {
            build.data()[w] |= (input.data()[w] ^ prev_input.data()[w]);
        }
    }
}

/**
 * @brief Update mask vector (CCSDS Equation 7).
 *
 * The mask vector identifies unpredictable bits (1 = unpredictable).
 *
 * - M_t = (I_t XOR I_{t-1}) OR M_{t-1} (if new_mask_flag = 0)
 * - M_t = (I_t XOR I_{t-1}) OR B_{t-1} (if new_mask_flag = 1)
 *
 * @tparam N BitVector length
 * @param[in,out] mask Updated mask vector M_t
 * @param[in] input Current input I_t
 * @param[in] prev_input Previous input I_{t-1}
 * @param[in] build_prev Previous build vector B_{t-1}
 * @param[in] new_mask_flag p_t flag (true = update from build)
 */
template <std::size_t N>
void update_mask(BitVector<N>& mask, const BitVector<N>& input, const BitVector<N>& prev_input,
                 const BitVector<N>& build_prev, bool new_mask_flag) noexcept {
    // Optimized: compute in-place without temporary allocation
    if (new_mask_flag) {
        // Case 1: new_mask_flag set -> M_t = (I_t XOR I_{t-1}) OR B_{t-1}
        for (std::size_t w = 0; w < BitVector<N>::NUM_WORDS; ++w) {
            mask.data()[w] = (input.data()[w] ^ prev_input.data()[w]) | build_prev.data()[w];
        }
    } else {
        // Case 2: Normal operation -> M_t = (I_t XOR I_{t-1}) OR M_{t-1}
        for (std::size_t w = 0; w < BitVector<N>::NUM_WORDS; ++w) {
            mask.data()[w] |= (input.data()[w] ^ prev_input.data()[w]);
        }
    }
}

/**
 * @brief Compute change vector (CCSDS Equation 8).
 *
 * The change vector tracks mask changes between iterations.
 *
 * - D_t = M_t XOR M_{t-1} (if t > 0)
 * - D_t = M_t (if t = 0, assuming M_{-1} = 0)
 *
 * @tparam N BitVector length
 * @param[out] change Change vector D_t
 * @param[in] mask Current mask M_t
 * @param[in] prev_mask Previous mask M_{t-1}
 * @param[in] t Time index (0 = first packet)
 */
template <std::size_t N>
void compute_change(BitVector<N>& change, const BitVector<N>& mask, const BitVector<N>& prev_mask,
                    std::size_t t) noexcept {
    if (t == 0) {
        // CCSDS Eq. 8: D_0 = 0 — no change at initialization; both encoder
        // and decoder start from the same user-specified M_0
        change.zero();
    } else {
        // D_t = M_t XOR M_{t-1}
        change.xor_with(mask, prev_mask);
    }
}

} // namespace pocketplus

#endif // POCKETPLUS_MASK_HPP

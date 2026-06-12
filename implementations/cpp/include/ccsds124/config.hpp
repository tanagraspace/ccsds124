/**
 * @file config.hpp
 * @brief CCSDS 124.0-B-1 compile-time configuration.
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
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#ifndef CCSDS124_CONFIG_HPP
#define CCSDS124_CONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace ccsds124 {

/**
 * @defgroup version Version Information
 * @{
 */
inline constexpr int VERSION_MAJOR = 1;
inline constexpr int VERSION_MINOR = 0;
inline constexpr int VERSION_PATCH = 0;
/** @} */

/**
 * @defgroup config Configuration Constants
 * @{
 */

/// Maximum packet length in bits (CCSDS max: 65535)
#ifndef CCSDS124_MAX_PACKET_LENGTH
#define CCSDS124_MAX_PACKET_LENGTH 65535U
#endif

inline constexpr std::size_t MAX_PACKET_LENGTH = CCSDS124_MAX_PACKET_LENGTH;
inline constexpr std::size_t MAX_PACKET_BYTES = (MAX_PACKET_LENGTH + 7U) / 8U;
inline constexpr std::size_t MAX_ROBUSTNESS = 7U;
inline constexpr std::size_t MAX_HISTORY = 16U;
inline constexpr std::size_t MAX_VT_HISTORY = 16U;
inline constexpr std::size_t MAX_OUTPUT_BYTES = MAX_PACKET_BYTES * 6U;

/// 32-bit word type for bit vector storage (matches C implementation)
using word_t = std::uint32_t;
inline constexpr std::size_t BITS_PER_WORD = 32U;

/** @} */

/**
 * @defgroup exceptions Exception Configuration
 *
 * Define CCSDS124_NO_EXCEPTIONS=1 to disable exceptions for embedded use.
 * @{
 */
#ifndef CCSDS124_NO_EXCEPTIONS
#define CCSDS124_NO_EXCEPTIONS 0
#endif

#ifndef CCSDS124_NO_RTTI
#define CCSDS124_NO_RTTI 0
#endif
/** @} */

} // namespace ccsds124

#endif // CCSDS124_CONFIG_HPP

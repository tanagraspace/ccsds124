/**
 * @file encoder.cpp
 * @brief Encoder compilation unit.
 *
 * This file exists for library structure purposes. The encoding functions
 * (COUNT, RLE, BE) are implemented entirely in the header file (encoder.hpp)
 * because:
 *
 * 1. Encoder functions are templates parameterized by buffer/vector sizes
 * 2. C++ requires template implementations to be visible at instantiation
 * 3. Header-only design is standard for template-heavy libraries
 *
 * Encoding functions implemented in encoder.hpp:
 * - count_encode() - CCSDS 124.0-B-1 Section 5.2.2, Equation 9
 * - rle_encode()   - CCSDS 124.0-B-1 Section 5.2.3, Equation 10
 * - bit_extract()  - CCSDS 124.0-B-1 Section 5.2.4, Equation 11
 *
 * This .cpp file:
 * - Includes the header to verify it compiles correctly in isolation
 * - Provides a compilation unit for the static library
 * - Can hold non-template utilities if needed in the future
 *
 * @see include/ccsds124/encoder.hpp for the full implementation
 */

#include <ccsds124/encoder.hpp>

// All implementation is in the header (template functions)

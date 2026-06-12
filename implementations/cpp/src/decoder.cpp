/**
 * @file decoder.cpp
 * @brief Decoder compilation unit.
 *
 * This file exists for library structure purposes. The decoding functions
 * are implemented entirely in the header file (decoder.hpp) because:
 *
 * 1. Decoder functions are templates parameterized by buffer/vector sizes
 * 2. C++ requires template implementations to be visible at instantiation
 * 3. Header-only design is standard for template-heavy libraries
 *
 * Decoding functions implemented in decoder.hpp:
 * - count_decode() - Inverse of CCSDS 124.0-B-1 Section 5.2.2
 * - rle_decode()   - Inverse of CCSDS 124.0-B-1 Section 5.2.3
 * - bit_insert()   - Inverse of CCSDS 124.0-B-1 Section 5.2.4
 *
 * This .cpp file:
 * - Includes the header to verify it compiles correctly in isolation
 * - Provides a compilation unit for the static library
 * - Can hold non-template utilities if needed in the future
 *
 * @see include/ccsds124/decoder.hpp for the full implementation
 */

#include <ccsds124/decoder.hpp>

// All implementation is in the header (template functions)

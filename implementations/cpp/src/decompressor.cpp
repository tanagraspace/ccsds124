/**
 * @file decompressor.cpp
 * @brief Decompressor compilation unit.
 *
 * This file exists for library structure purposes. The Decompressor class is
 * implemented entirely in the header file (decompressor.hpp) because:
 *
 * 1. Decompressor is a template class parameterized by packet bit length N
 * 2. C++ requires template implementations to be visible at instantiation
 * 3. Header-only design is standard for template-heavy libraries
 *
 * The Decompressor class implements the inverse of CCSDS 124.0-B-1 Section 5.3:
 * - Stateful decompression with mask reconstruction
 * - Parsing of h_t (mask changes) and q_t (optional full mask)
 * - Full round-trip support with Compressor
 *
 * This .cpp file:
 * - Includes the header to verify it compiles correctly in isolation
 * - Provides a compilation unit for the static library
 * - Can hold non-template utilities if needed in the future
 *
 * @see include/ccsds124/decompressor.hpp for the full implementation
 */

#include <ccsds124/decompressor.hpp>

// All implementation is in the header (template class)

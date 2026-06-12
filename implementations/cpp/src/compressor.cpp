/**
 * @file compressor.cpp
 * @brief Compressor compilation unit.
 *
 * This file exists for library structure purposes. The Compressor class is
 * implemented entirely in the header file (compressor.hpp) because:
 *
 * 1. Compressor is a template class parameterized by packet bit length N
 * 2. C++ requires template implementations to be visible at instantiation
 * 3. Header-only design is standard for template-heavy libraries
 *
 * The Compressor class implements CCSDS 124.0-B-1 Section 5.3:
 * - Stateful compression with mask tracking
 * - Automatic parameter computation (P_t, F_t, R_t limits)
 * - Support for robustness levels 0-7
 *
 * This .cpp file:
 * - Includes the header to verify it compiles correctly in isolation
 * - Provides a compilation unit for the static library
 * - Can hold non-template utilities if needed in the future
 *
 * @see include/ccsds124/compressor.hpp for the full implementation
 */

#include <ccsds124/compressor.hpp>

// All implementation is in the header (template class)

/**
 * @file bitvector.cpp
 * @brief BitVector compilation unit.
 *
 * This file exists for library structure purposes. The BitVector class is
 * implemented entirely in the header file (bitvector.hpp) because:
 *
 * 1. BitVector is a template class parameterized by bit length N
 * 2. C++ requires template implementations to be visible at instantiation
 * 3. Header-only design is standard for template-heavy libraries
 *
 * Benefits of header-only templates:
 * - Compiler can inline and optimize for specific template parameters
 * - No need for explicit template instantiation
 * - Simpler build configuration
 *
 * This .cpp file:
 * - Includes the header to verify it compiles correctly in isolation
 * - Provides a compilation unit for the static library
 * - Can hold non-template utilities if needed in the future
 *
 * @see include/ccsds124/bitvector.hpp for the full implementation
 */

#include <ccsds124/bitvector.hpp>

// All implementation is in the header (template class)

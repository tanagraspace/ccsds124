/**
 * @file bitreader.cpp
 * @brief BitReader compilation unit.
 *
 * This file exists for library structure purposes. The BitReader class is
 * implemented entirely in the header file (bitreader.hpp) because:
 *
 * 1. BitReader uses inline functions for performance-critical bit operations
 * 2. Header-only design allows full inlining by the compiler
 * 3. Consistent with the rest of the library's header-only approach
 *
 * Benefits of header-only implementation:
 * - Compiler can inline all bit manipulation operations
 * - No function call overhead for tight loops
 * - Simpler build configuration
 *
 * This .cpp file:
 * - Includes the header to verify it compiles correctly in isolation
 * - Provides a compilation unit for the static library
 * - Can hold non-inline utilities if needed in the future
 *
 * @see include/ccsds124/bitreader.hpp for the full implementation
 */

#include <ccsds124/bitreader.hpp>

// All implementation is in the header (inline functions)

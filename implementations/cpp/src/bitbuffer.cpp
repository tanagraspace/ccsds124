/**
 * @file bitbuffer.cpp
 * @brief BitBuffer compilation unit.
 *
 * This file exists for library structure purposes. The BitBuffer class is
 * implemented entirely in the header file (bitbuffer.hpp) because:
 *
 * 1. BitBuffer is a template class parameterized by maximum byte capacity
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
 * @see include/ccsds124/bitbuffer.hpp for the full implementation
 */

#include <ccsds124/bitbuffer.hpp>

// All implementation is in the header (template class)

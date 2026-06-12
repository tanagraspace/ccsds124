/**
 * @file test_mask.cpp
 * @brief Unit tests for CCSDS 124.0-B-1 mask update logic.
 *
 * Tests for CCSDS 124.0-B-1 Section 4:
 * - Update build vector (Equation 6)
 * - Update mask vector (Equation 7)
 * - Compute change vector (Equation 8)
 */

#include <ccsds124/bitvector.hpp>
#include <ccsds124/mask.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace ccsds124;

// ============================================================================
// Update Build Vector Tests (CCSDS Equation 6)
//
// B_t = (I_t XOR I_{t-1}) OR B_{t-1}  (if t > 0 and new_mask_flag = 0)
// B_t = 0                             (otherwise: t=0 or new_mask_flag=1)
// ============================================================================

TEST_CASE("Update build at t=0", "[mask][build]") {
    // At t=0, B_t should always be 0 regardless of inputs
    BitVector<8> build;
    BitVector<8> input;
    BitVector<8> prev_input;

    // Set some initial values
    for (std::size_t i = 0; i < 8; ++i) {
        build.set_bit(i, 1);
    }
    input.set_bit(0, 1);
    input.set_bit(2, 1);
    prev_input.set_bit(1, 1);
    prev_input.set_bit(3, 1);

    update_build(build, input, prev_input, false, 0);

    // At t=0, build should be cleared to 0
    REQUIRE(build.hamming_weight() == 0);
}

TEST_CASE("Update build with new_mask_flag", "[mask][build]") {
    // When new_mask_flag = 1, B_t should be 0
    BitVector<8> build;
    BitVector<8> input;
    BitVector<8> prev_input;

    // Set all bits in build
    for (std::size_t i = 0; i < 8; ++i) {
        build.set_bit(i, 1);
    }
    input.set_bit(0, 1);
    prev_input.set_bit(1, 1);

    update_build(build, input, prev_input, true, 5);

    // With new_mask_flag=true, build should be cleared
    REQUIRE(build.hamming_weight() == 0);
}

TEST_CASE("Update build normal case", "[mask][build]") {
    // Normal case: t > 0 and new_mask_flag = 0
    // B_t = (I_t XOR I_{t-1}) OR B_{t-1}
    BitVector<8> build;
    BitVector<8> input;
    BitVector<8> prev_input;

    // Simple test: input differs from prev_input at bits 0 and 7
    // Build already has bit 3 set
    // Result should have bits 0, 3, 7 set

    input.set_bit(0, 1);
    input.set_bit(1, 1);
    input.set_bit(7, 1);

    prev_input.set_bit(1, 1); // Same as input at bit 1

    // Build starts with bit 3
    build.set_bit(3, 1);

    update_build(build, input, prev_input, false, 1);

    // XOR: bits 0 and 7 differ
    // OR with build: bits 0, 3, 7 should be set
    REQUIRE(build.get_bit(0) == 1);
    REQUIRE(build.get_bit(3) == 1);
    REQUIRE(build.get_bit(7) == 1);
    REQUIRE(build.hamming_weight() == 3);
}

TEST_CASE("Update build no change", "[mask][build]") {
    // If I_t = I_{t-1}, then XOR yields 0, so B_t = B_{t-1}
    BitVector<8> build;
    BitVector<8> input;
    BitVector<8> prev_input;

    // Set same pattern for input and prev_input
    input.set_bit(0, 1);
    input.set_bit(2, 1);
    input.set_bit(4, 1);
    input.set_bit(6, 1);
    prev_input.set_bit(0, 1);
    prev_input.set_bit(2, 1);
    prev_input.set_bit(4, 1);
    prev_input.set_bit(6, 1);

    // Set build = 01010101
    build.set_bit(1, 1);
    build.set_bit(3, 1);
    build.set_bit(5, 1);
    build.set_bit(7, 1);

    std::uint32_t original_weight = build.hamming_weight();

    update_build(build, input, prev_input, false, 3);

    // XOR of identical values is 0, so build stays the same
    REQUIRE(build.hamming_weight() == original_weight);
    REQUIRE(build.get_bit(1) == 1);
    REQUIRE(build.get_bit(3) == 1);
    REQUIRE(build.get_bit(5) == 1);
    REQUIRE(build.get_bit(7) == 1);
}

// ============================================================================
// Update Mask Vector Tests (CCSDS Equation 7)
//
// M_t = (I_t XOR I_{t-1}) OR M_{t-1}    (if new_mask_flag = 0)
// M_t = (I_t XOR I_{t-1}) OR B_{t-1}    (if new_mask_flag = 1)
// ============================================================================

TEST_CASE("Update mask normal case", "[mask][mask_update]") {
    // Normal case: new_mask_flag = 0
    // M_t = (I_t XOR I_{t-1}) OR M_{t-1}
    BitVector<8> mask;
    BitVector<8> input;
    BitVector<8> prev_input;
    BitVector<8> build_prev;

    // Example:
    // I_t     = 11110000 = 0xF0
    // I_{t-1} = 10100000 = 0xA0
    // M_{t-1} = 01000001 = 0x41
    //
    // I_t XOR I_{t-1} = 01010000 = 0x50
    // (I_t XOR I_{t-1}) OR M_{t-1} = 01010000 OR 01000001 = 01010001 = 0x51

    // Set input = 0xF0 = 11110000
    input.set_bit(0, 1);
    input.set_bit(1, 1);
    input.set_bit(2, 1);
    input.set_bit(3, 1);

    // Set prev_input = 0xA0 = 10100000
    prev_input.set_bit(0, 1);
    prev_input.set_bit(2, 1);

    // Set mask = 0x41 = 01000001
    mask.set_bit(1, 1);
    mask.set_bit(7, 1);

    // Set build_prev (not used when new_mask_flag=false)
    for (std::size_t i = 0; i < 8; ++i) {
        build_prev.set_bit(i, 1);
    }

    update_mask(mask, input, prev_input, build_prev, false);

    // Expected: 0x51 = 01010001
    REQUIRE(mask.get_bit(1) == 1);
    REQUIRE(mask.get_bit(3) == 1);
    REQUIRE(mask.get_bit(7) == 1);
    REQUIRE(mask.hamming_weight() == 3);
}

TEST_CASE("Update mask with new_mask_flag", "[mask][mask_update]") {
    // When new_mask_flag = 1:
    // M_t = (I_t XOR I_{t-1}) OR B_{t-1}
    BitVector<8> mask;
    BitVector<8> input;
    BitVector<8> prev_input;
    BitVector<8> build_prev;

    // Example:
    // I_t     = 11110000 = 0xF0
    // I_{t-1} = 10100000 = 0xA0
    // B_{t-1} = 00001111 = 0x0F
    //
    // I_t XOR I_{t-1} = 01010000 = 0x50
    // (I_t XOR I_{t-1}) OR B_{t-1} = 01010000 OR 00001111 = 01011111 = 0x5F

    // Set input = 0xF0 = 11110000
    input.set_bit(0, 1);
    input.set_bit(1, 1);
    input.set_bit(2, 1);
    input.set_bit(3, 1);

    // Set prev_input = 0xA0 = 10100000
    prev_input.set_bit(0, 1);
    prev_input.set_bit(2, 1);

    // Set mask = 0x41 (old mask, will be replaced)
    mask.set_bit(1, 1);
    mask.set_bit(7, 1);

    // Set build_prev = 0x0F = 00001111
    build_prev.set_bit(4, 1);
    build_prev.set_bit(5, 1);
    build_prev.set_bit(6, 1);
    build_prev.set_bit(7, 1);

    update_mask(mask, input, prev_input, build_prev, true);

    // Expected: 0x5F = 01011111
    REQUIRE(mask.get_bit(1) == 1);
    REQUIRE(mask.get_bit(3) == 1);
    REQUIRE(mask.get_bit(4) == 1);
    REQUIRE(mask.get_bit(5) == 1);
    REQUIRE(mask.get_bit(6) == 1);
    REQUIRE(mask.get_bit(7) == 1);
    REQUIRE(mask.hamming_weight() == 6);
}

TEST_CASE("Update mask no change", "[mask][mask_update]") {
    // If I_t = I_{t-1}, XOR yields 0
    BitVector<8> mask;
    BitVector<8> input;
    BitVector<8> prev_input;
    BitVector<8> build_prev;

    // Set same pattern for input and prev_input
    input.set_bit(0, 1);
    input.set_bit(1, 1);
    prev_input.set_bit(0, 1);
    prev_input.set_bit(1, 1);

    // Set mask = 0x33 = 00110011
    mask.set_bit(2, 1);
    mask.set_bit(3, 1);
    mask.set_bit(6, 1);
    mask.set_bit(7, 1);

    std::uint32_t original_weight = mask.hamming_weight();

    update_mask(mask, input, prev_input, build_prev, false);

    // XOR is 0, so mask OR 0 = mask = unchanged
    REQUIRE(mask.hamming_weight() == original_weight);
}

// ============================================================================
// Compute Change Vector Tests (CCSDS Equation 8)
//
// D_t = M_t XOR M_{t-1}  (if t > 0)
// D_t = M_t              (if t = 0, assuming M_{-1} = 0)
// ============================================================================

TEST_CASE("Compute change at t=0", "[mask][change]") {
    // CCSDS Eq. 8: D_0 = 0 regardless of M_0 (no change at initialization)
    BitVector<8> change;
    BitVector<8> mask;
    BitVector<8> prev_mask;

    // Set mask = 0xFF (all ones) — must NOT leak into D_0
    for (std::size_t i = 0; i < 8; ++i) {
        mask.set_bit(i, 1);
    }

    // Set prev_mask (should be ignored at t=0)
    prev_mask.set_bit(0, 1);
    prev_mask.set_bit(2, 1);

    // Set change to some initial value (should be overwritten)
    change.set_bit(1, 1);

    compute_change(change, mask, prev_mask, 0);

    // CCSDS Eq. 8: D_0 = 0
    REQUIRE(change.hamming_weight() == 0);
}

TEST_CASE("Compute change normal case", "[mask][change]") {
    // Normal case: t > 0
    // D_t = M_t XOR M_{t-1}
    BitVector<8> change;
    BitVector<8> mask;
    BitVector<8> prev_mask;

    // Example:
    // M_t   = 11001100 = 0xCC
    // M_{t-1} = 10101010 = 0xAA
    //
    // M_t XOR M_{t-1} = 01100110 = 0x66

    // Set mask = 0xCC = 11001100
    mask.set_bit(0, 1);
    mask.set_bit(1, 1);
    mask.set_bit(4, 1);
    mask.set_bit(5, 1);

    // Set prev_mask = 0xAA = 10101010
    prev_mask.set_bit(0, 1);
    prev_mask.set_bit(2, 1);
    prev_mask.set_bit(4, 1);
    prev_mask.set_bit(6, 1);

    compute_change(change, mask, prev_mask, 1);

    // Expected: 0x66 = 01100110
    REQUIRE(change.get_bit(1) == 1);
    REQUIRE(change.get_bit(2) == 1);
    REQUIRE(change.get_bit(5) == 1);
    REQUIRE(change.get_bit(6) == 1);
    REQUIRE(change.hamming_weight() == 4);
}

TEST_CASE("Compute change no mask change", "[mask][change]") {
    // If M_t = M_{t-1}, then D_t = 0
    BitVector<8> change;
    BitVector<8> mask;
    BitVector<8> prev_mask;

    // Set same mask and prev_mask
    mask.set_bit(0, 1);
    mask.set_bit(2, 1);
    mask.set_bit(4, 1);
    prev_mask.set_bit(0, 1);
    prev_mask.set_bit(2, 1);
    prev_mask.set_bit(4, 1);

    // Set change to some initial value
    for (std::size_t i = 0; i < 8; ++i) {
        change.set_bit(i, 1);
    }

    compute_change(change, mask, prev_mask, 5);

    // XOR of identical values is 0
    REQUIRE(change.hamming_weight() == 0);
}

TEST_CASE("Compute change all bits flip", "[mask][change]") {
    // All bits change
    BitVector<8> change;
    BitVector<8> mask;
    BitVector<8> prev_mask;

    // Set mask = 0xFF (all ones)
    for (std::size_t i = 0; i < 8; ++i) {
        mask.set_bit(i, 1);
    }

    // Set prev_mask = 0x00 (all zeros)
    // (BitVector initializes to zero)

    compute_change(change, mask, prev_mask, 2);

    // All bits flipped, so XOR yields all 1s
    REQUIRE(change.hamming_weight() == 8);
}

// ============================================================================
// Integration Tests - Multiple Updates in Sequence
// ============================================================================

TEST_CASE("Mask update sequence", "[mask][integration]") {
    // Test a sequence of updates to ensure state transitions work correctly
    BitVector<8> build;
    BitVector<8> mask;
    BitVector<8> change;
    BitVector<8> prev_mask;
    BitVector<8> input_t0;
    BitVector<8> input_t1;
    BitVector<8> input_t2;

    // Initial state: all zeros (BitVector initializes to zero)

    // t=0: First input = 0xAA = 10101010
    input_t0.set_bit(0, 1);
    input_t0.set_bit(2, 1);
    input_t0.set_bit(4, 1);
    input_t0.set_bit(6, 1);

    // At t=0, update_build resets to 0
    update_build(build, input_t0, input_t0, false, 0);
    REQUIRE(build.hamming_weight() == 0);

    // At t=0, compute_change returns mask (which is 0)
    compute_change(change, mask, prev_mask, 0);
    REQUIRE(change.hamming_weight() == 0);

    // t=1: Second input = 0xCC = 11001100
    input_t1.set_bit(0, 1);
    input_t1.set_bit(1, 1);
    input_t1.set_bit(4, 1);
    input_t1.set_bit(5, 1);

    // Save previous mask
    prev_mask = mask;

    // Update build: B_t = (I_t XOR I_{t-1}) OR B_{t-1}
    // 0xCC XOR 0xAA = 0x66 = 01100110
    // 0x66 OR 0x00 = 0x66
    update_build(build, input_t1, input_t0, false, 1);
    REQUIRE(build.get_bit(1) == 1);
    REQUIRE(build.get_bit(2) == 1);
    REQUIRE(build.get_bit(5) == 1);
    REQUIRE(build.get_bit(6) == 1);
    REQUIRE(build.hamming_weight() == 4);

    // Update mask: M_t = (I_t XOR I_{t-1}) OR M_{t-1}
    // 0x66 OR 0x00 = 0x66
    update_mask(mask, input_t1, input_t0, build, false);
    REQUIRE(mask.hamming_weight() == 4);

    // Compute change: D_t = M_t XOR M_{t-1}
    // 0x66 XOR 0x00 = 0x66
    compute_change(change, mask, prev_mask, 1);
    REQUIRE(change.hamming_weight() == 4);

    // t=2: Third input = same as t=1 (0xCC)
    input_t2 = input_t1;
    prev_mask = mask;

    // Update build: B_t = (I_t XOR I_{t-1}) OR B_{t-1}
    // 0xCC XOR 0xCC = 0x00
    // 0x00 OR 0x66 = 0x66
    update_build(build, input_t2, input_t1, false, 2);
    REQUIRE(build.hamming_weight() == 4);

    // Update mask: M_t = (I_t XOR I_{t-1}) OR M_{t-1}
    // 0x00 OR 0x66 = 0x66
    update_mask(mask, input_t2, input_t1, build, false);
    REQUIRE(mask.hamming_weight() == 4);

    // Compute change: D_t = M_t XOR M_{t-1}
    // 0x66 XOR 0x66 = 0x00 (no change)
    compute_change(change, mask, prev_mask, 2);
    REQUIRE(change.hamming_weight() == 0);
}

TEST_CASE("Mask reset with new_mask_flag", "[mask][integration]") {
    // Test mask reset when new_mask_flag is set
    BitVector<8> build;
    BitVector<8> mask;
    BitVector<8> input;
    BitVector<8> prev_input;

    // Set up initial state with some values
    for (std::size_t i = 0; i < 8; i += 2) {
        build.set_bit(i, 1);
        mask.set_bit(i, 1);
    }

    input.set_bit(0, 1);
    prev_input.set_bit(1, 1);

    // Reset with new_mask_flag
    update_build(build, input, prev_input, true, 5);
    REQUIRE(build.hamming_weight() == 0);

    // Update mask with new_mask_flag uses build_prev
    BitVector<8> build_prev;
    build_prev.set_bit(7, 1);

    update_mask(mask, input, prev_input, build_prev, true);
    // M_t = (I_t XOR I_{t-1}) OR B_{t-1}
    // (0x01 XOR 0x02) OR 0x01 = 0x03 OR 0x01 = 0x03
    REQUIRE(mask.get_bit(0) == 1);
    REQUIRE(mask.get_bit(1) == 1);
    REQUIRE(mask.get_bit(7) == 1);
}

/**
 * @file test_compressor.cpp
 * @brief Unit tests for Compressor class.
 */

#include <ccsds124/bitbuffer.hpp>
#include <ccsds124/bitvector.hpp>
#include <ccsds124/compressor.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace ccsds124;

TEST_CASE("Compressor construction", "[compressor]") {
    SECTION("default construction") {
        Compressor<8> comp;
        REQUIRE(comp.time_index() == 0);
        REQUIRE(comp.robustness() == 0);
    }

    SECTION("with robustness") {
        Compressor<8> comp(3);
        REQUIRE(comp.robustness() == 3);
    }

    SECTION("robustness capped at max") {
        Compressor<8> comp(10); // Should be capped to 7
        REQUIRE(comp.robustness() == 7);
    }

    SECTION("with auto parameters") {
        Compressor<8> comp(2, 10, 20, 50);
        REQUIRE(comp.robustness() == 2);
    }
}

TEST_CASE("Compressor reset", "[compressor]") {
    Compressor<8> comp(2);
    BitVector<8> input;
    BitBuffer<64> output;

    // Compress one packet to change state
    input.set_bit(0, 1);
    auto result = comp.compress_packet(input, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(comp.time_index() == 1);

    // Reset should clear state
    comp.reset();
    REQUIRE(comp.time_index() == 0);
}

TEST_CASE("Compressor first packet", "[compressor]") {
    Compressor<8> comp;
    BitVector<8> input;
    BitBuffer<64> output;

    // First packet should always produce output
    input.set_bit(0, 1);
    input.set_bit(3, 1);

    auto result = comp.compress_packet(input, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(output.size() > 0);
    REQUIRE(comp.time_index() == 1);
}

TEST_CASE("Compressor multiple packets", "[compressor]") {
    Compressor<8> comp;
    BitVector<8> input;
    BitBuffer<64> output;

    // First packet
    input.set_bit(0, 1);
    auto result = comp.compress_packet(input, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(comp.time_index() == 1);

    // Second packet - same data (predictable)
    output.clear();
    result = comp.compress_packet(input, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(comp.time_index() == 2);

    // Third packet - different data
    input.set_bit(1, 1);
    output.clear();
    result = comp.compress_packet(input, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(comp.time_index() == 3);
}

TEST_CASE("Compressor with initial mask", "[compressor]") {
    Compressor<8> comp;
    BitVector<8> initial_mask;

    // Set some bits as unpredictable
    initial_mask.set_bit(0, 1);
    initial_mask.set_bit(7, 1);

    comp.set_initial_mask(initial_mask);
    comp.reset(); // Apply the initial mask

    BitVector<8> input;
    BitBuffer<64> output;

    input.set_bit(0, 1);
    auto result = comp.compress_packet(input, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(output.size() > 0);
}

TEST_CASE("Compressor with params", "[compressor]") {
    Compressor<8> comp(2);
    BitVector<8> input;
    BitBuffer<256> output;

    CompressParams params;
    params.min_robustness = 2;
    params.new_mask_flag = false;
    params.send_mask_flag = true;
    params.uncompressed_flag = true;

    input.set_bit(0, 1);
    auto result = comp.compress_packet(input, output, &params);
    REQUIRE(result == Error::Ok);
    REQUIRE(output.size() > 0);
}

TEST_CASE("Compressor auto params computation", "[compressor]") {
    Compressor<8> comp(2, 10, 20, 50);

    SECTION("first packet") {
        auto params = comp.compute_auto_params(0);
        REQUIRE(params.send_mask_flag == true);
        REQUIRE(params.uncompressed_flag == true);
        REQUIRE(params.new_mask_flag == false);
    }

    SECTION("init phase packets") {
        // Packets 0 through robustness (2) should have ft=1, rt=1
        for (std::size_t i = 0; i <= 2; ++i) {
            auto params = comp.compute_auto_params(i);
            REQUIRE(params.send_mask_flag == true);
            REQUIRE(params.uncompressed_flag == true);
        }
    }
}

TEST_CASE("Compressor uncompressed mode", "[compressor]") {
    Compressor<8> comp;
    BitVector<8> input;
    BitBuffer<256> output;

    CompressParams params;
    params.uncompressed_flag = true;

    // Set some data
    input.set_bit(0, 1);
    input.set_bit(2, 1);
    input.set_bit(4, 1);

    auto result = comp.compress_packet(input, output, &params);
    REQUIRE(result == Error::Ok);
    // Uncompressed output includes the full input + header
    REQUIRE(output.size() > 8);
}

TEST_CASE("Compressor with robustness", "[compressor]") {
    Compressor<8> comp(3); // Robustness = 3
    BitVector<8> input;
    BitBuffer<128> output;

    // Process several packets to build history
    for (int i = 0; i < 5; ++i) {
        input.zero();
        input.set_bit(static_cast<std::size_t>(i % 8), 1);

        output.clear();
        auto result = comp.compress_packet(input, output);
        REQUIRE(result == Error::Ok);
    }

    REQUIRE(comp.time_index() == 5);
}

TEST_CASE("Mask update functions", "[compressor][mask]") {
    SECTION("update_build resets on new_mask_flag") {
        BitVector<8> build;
        BitVector<8> input;
        BitVector<8> prev_input;

        // Set build to non-zero
        build.set_bit(0, 1);

        // With new_mask_flag = true, build should reset to zero
        update_build(build, input, prev_input, true, 1);
        REQUIRE(build.hamming_weight() == 0);
    }

    SECTION("update_build accumulates changes") {
        BitVector<8> build;
        BitVector<8> input;
        BitVector<8> prev_input;

        // First packet has bit 0 set
        prev_input.set_bit(0, 1);
        // Second packet has bit 1 set
        input.set_bit(1, 1);

        // Build should now have bits 0 and 1 (XOR gives both)
        update_build(build, input, prev_input, false, 1);
        REQUIRE(build.get_bit(0) == 1);
        REQUIRE(build.get_bit(1) == 1);
    }

    SECTION("update_mask normal mode") {
        BitVector<8> mask;
        BitVector<8> input;
        BitVector<8> prev_input;
        BitVector<8> build_prev;

        // Mask starts with bit 0 set
        mask.set_bit(0, 1);
        // Input changes at bit 1
        input.set_bit(1, 1);

        update_mask(mask, input, prev_input, build_prev, false);

        // Mask should now have bits 0 and 1
        REQUIRE(mask.get_bit(0) == 1);
        REQUIRE(mask.get_bit(1) == 1);
    }

    SECTION("update_mask with new_mask_flag") {
        BitVector<8> mask;
        BitVector<8> input;
        BitVector<8> prev_input;
        BitVector<8> build_prev;

        // Build has bit 2 set
        build_prev.set_bit(2, 1);
        // Input changes at bit 1
        input.set_bit(1, 1);

        update_mask(mask, input, prev_input, build_prev, true);

        // Mask should be (changes OR build_prev)
        REQUIRE(mask.get_bit(1) == 1);
        REQUIRE(mask.get_bit(2) == 1);
    }

    SECTION("compute_change at t=0") {
        BitVector<8> change;
        BitVector<8> mask;
        BitVector<8> prev_mask;

        mask.set_bit(0, 1);

        compute_change(change, mask, prev_mask, 0);

        // CCSDS Eq. 8: D_0 = 0 regardless of M_0 (no change at initialization)
        REQUIRE(change.hamming_weight() == 0);
    }

    SECTION("compute_change at t>0") {
        BitVector<8> change;
        BitVector<8> mask;
        BitVector<8> prev_mask;

        mask.set_bit(0, 1);
        mask.set_bit(1, 1);
        prev_mask.set_bit(0, 1);

        compute_change(change, mask, prev_mask, 1);

        // Change = mask XOR prev_mask
        REQUIRE(change.get_bit(0) == 0);
        REQUIRE(change.get_bit(1) == 1);
    }
}

TEST_CASE("Compressor consistency", "[compressor]") {
    // Compressing the same data should be deterministic
    Compressor<8> comp1;
    Compressor<8> comp2;

    BitVector<8> input;
    input.set_bit(0, 1);
    input.set_bit(3, 1);

    BitBuffer<64> output1;
    BitBuffer<64> output2;

    comp1.compress_packet(input, output1);
    comp2.compress_packet(input, output2);

    // Both compressors should produce identical output
    REQUIRE(output1.size() == output2.size());

    std::uint8_t bytes1[8] = {0};
    std::uint8_t bytes2[8] = {0};
    output1.to_bytes(bytes1, 8);
    output2.to_bytes(bytes2, 8);

    for (std::size_t i = 0; i < output1.size() / 8; ++i) {
        REQUIRE(bytes1[i] == bytes2[i]);
    }
}

TEST_CASE("Compressor larger packet size", "[compressor]") {
    Compressor<720> comp(2); // 90 bytes = 720 bits (test vector size)
    BitVector<720> input;
    BitBuffer<720 * 6> output;

    // Set some data
    for (std::size_t i = 0; i < 720; i += 8) {
        input.set_bit(i, 1);
    }

    auto result = comp.compress_packet(input, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(output.size() > 0);
}

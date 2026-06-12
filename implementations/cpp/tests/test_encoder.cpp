/**
 * @file test_encoder.cpp
 * @brief Unit tests for Encoder functions (COUNT, RLE, BE).
 */

#include <ccsds124/bitbuffer.hpp>
#include <ccsds124/bitvector.hpp>
#include <ccsds124/encoder.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace ccsds124;

TEST_CASE("COUNT encoding A=1", "[encoder]") {
    BitBuffer<64> bb;

    // A=1 -> '0' (1 bit)
    auto result = count_encode(bb, 1);
    REQUIRE(result == Error::Ok);
    REQUIRE(bb.size() == 1);

    std::uint8_t output[1] = {0};
    bb.to_bytes(output, 1);
    REQUIRE((output[0] & 0x80) == 0x00); // First bit is 0
}

TEST_CASE("COUNT encoding A=2-33", "[encoder]") {
    SECTION("A=2") {
        BitBuffer<64> bb;
        // A=2 -> '110' + BIT5(0) = '11000000' = 0xC0
        auto result = count_encode(bb, 2);
        REQUIRE(result == Error::Ok);
        REQUIRE(bb.size() == 8);

        std::uint8_t output[1] = {0};
        bb.to_bytes(output, 1);
        REQUIRE(output[0] == 0xC0);
    }

    SECTION("A=10") {
        BitBuffer<64> bb;
        // A=10 -> '110' + BIT5(8) = '110' + '01000' = '11001000' = 0xC8
        auto result = count_encode(bb, 10);
        REQUIRE(result == Error::Ok);
        REQUIRE(bb.size() == 8);

        std::uint8_t output[1] = {0};
        bb.to_bytes(output, 1);
        REQUIRE(output[0] == 0xC8);
    }

    SECTION("A=33") {
        BitBuffer<64> bb;
        // A=33 -> '110' + BIT5(31) = '110' + '11111' = '11011111' = 0xDF
        auto result = count_encode(bb, 33);
        REQUIRE(result == Error::Ok);
        REQUIRE(bb.size() == 8);

        std::uint8_t output[1] = {0};
        bb.to_bytes(output, 1);
        REQUIRE(output[0] == 0xDF);
    }
}

TEST_CASE("COUNT encoding A>=34", "[encoder]") {
    BitBuffer<64> bb;
    // A=34 -> '111' + BIT_E(32) where E = 2*floor(log2(32)+1) - 6 = 2*6-6 = 6
    // BIT_6(32) = '100000'
    // Full: '111100000' = 9 bits
    auto result = count_encode(bb, 34);
    REQUIRE(result == Error::Ok);
    REQUIRE(bb.size() == 9);
}

TEST_CASE("COUNT encoding invalid values", "[encoder]") {
    BitBuffer<64> bb;

    SECTION("A=0 is invalid") {
        auto result = count_encode(bb, 0);
        REQUIRE(result == Error::InvalidArg);
    }

    SECTION("A>65535 is invalid") {
        auto result = count_encode(bb, 65536);
        REQUIRE(result == Error::InvalidArg);
    }
}

TEST_CASE("RLE encoding all zeros", "[encoder]") {
    BitBuffer<128> bb;
    BitVector<8> input; // All zeros

    // RLE of all zeros = just terminator '10'
    auto result = rle_encode(bb, input);
    REQUIRE(result == Error::Ok);
    REQUIRE(bb.size() == 2); // Just the terminator

    std::uint8_t output[1] = {0};
    bb.to_bytes(output, 1);
    REQUIRE((output[0] & 0xC0) == 0x80); // '10' = 0x80
}

TEST_CASE("RLE encoding single bit", "[encoder]") {
    BitBuffer<128> bb;
    BitVector<8> input;
    input.set_bit(0, 1); // MSB set

    // One '1' bit at position 0 with 7 trailing zeros
    // COUNT(8) for the run (7 zeros + 1 for the bit) then terminator
    auto result = rle_encode(bb, input);
    REQUIRE(result == Error::Ok);
}

TEST_CASE("RLE encoding multiple bits", "[encoder]") {
    BitBuffer<256> bb;
    BitVector<8> input;
    input.set_bit(0, 1); // Position 0
    input.set_bit(7, 1); // Position 7 (LSB)

    auto result = rle_encode(bb, input);
    REQUIRE(result == Error::Ok);
    // Should encode two runs plus terminator
}

TEST_CASE("Bit extraction BE reverse order", "[encoder]") {
    BitBuffer<64> bb;
    BitVector<8> data;
    BitVector<8> mask;

    // Data: 10110011
    data.set_bit(0, 1);
    data.set_bit(2, 1);
    data.set_bit(3, 1);
    data.set_bit(6, 1);
    data.set_bit(7, 1);

    // Mask: 01001010 (extract bits at positions 1, 4, 6)
    mask.set_bit(1, 1);
    mask.set_bit(4, 1);
    mask.set_bit(6, 1);

    auto result = bit_extract(bb, data, mask);
    REQUIRE(result == Error::Ok);
    REQUIRE(bb.size() == 3); // 3 bits extracted
}

TEST_CASE("Bit extraction BE all masked", "[encoder]") {
    BitBuffer<64> bb;
    BitVector<8> data;
    BitVector<8> mask; // All zeros - no bits to extract

    auto result = bit_extract(bb, data, mask);
    REQUIRE(result == Error::Ok);
    REQUIRE(bb.size() == 0); // No bits extracted
}

TEST_CASE("Bit extraction forward order", "[encoder]") {
    BitBuffer<64> bb;
    BitVector<8> data;
    BitVector<8> mask;

    data.set_bit(0, 1);
    data.set_bit(7, 1);

    mask.set_bit(0, 1);
    mask.set_bit(7, 1);

    auto result = bit_extract_forward(bb, data, mask);
    REQUIRE(result == Error::Ok);
    REQUIRE(bb.size() == 2);

    std::uint8_t output[1] = {0};
    bb.to_bytes(output, 1);
    // Forward: bit 0 first, bit 7 second -> '11' = 0xC0
    REQUIRE((output[0] & 0xC0) == 0xC0);
}

TEST_CASE("Bit extraction length mismatch", "[encoder]") {
    BitBuffer<64> bb;
    BitVector<8> data;
    BitVector<16> mask; // Different length

    auto result = bit_extract(bb, data, mask);
    REQUIRE(result == Error::InvalidArg);
}

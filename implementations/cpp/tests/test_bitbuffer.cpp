/**
 * @file test_bitbuffer.cpp
 * @brief Unit tests for BitBuffer class.
 */

#include <ccsds124/bitbuffer.hpp>
#include <ccsds124/bitvector.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace ccsds124;

TEST_CASE("BitBuffer construction", "[bitbuffer]") {
    SECTION("default construction") {
        BitBuffer<256> bb;
        REQUIRE(bb.size() == 0);
    }
}

TEST_CASE("BitBuffer append_bit", "[bitbuffer]") {
    BitBuffer<64> bb;

    SECTION("append single bits") {
        bb.append_bit(1);
        REQUIRE(bb.size() == 1);

        bb.append_bit(0);
        bb.append_bit(1);
        bb.append_bit(1);
        REQUIRE(bb.size() == 4);
    }

    SECTION("append 8 bits and convert to bytes") {
        // 1010 0101 = 0xA5
        bb.append_bit(1);
        bb.append_bit(0);
        bb.append_bit(1);
        bb.append_bit(0);
        bb.append_bit(0);
        bb.append_bit(1);
        bb.append_bit(0);
        bb.append_bit(1);

        std::uint8_t output[1] = {0};
        std::size_t bytes = bb.to_bytes(output, 1);

        REQUIRE(bytes == 1);
        REQUIRE(output[0] == 0xA5);
    }
}

TEST_CASE("BitBuffer append_bits", "[bitbuffer]") {
    BitBuffer<128> bb;

    SECTION("append from byte array") {
        std::uint8_t data[] = {0xAB, 0xCD};
        bb.append_bits(data, 16);

        REQUIRE(bb.size() == 16);

        std::uint8_t output[2] = {0};
        bb.to_bytes(output, 2);

        REQUIRE(output[0] == 0xAB);
        REQUIRE(output[1] == 0xCD);
    }

    SECTION("append partial byte") {
        std::uint8_t data[] = {0xF0}; // 1111 0000
        bb.append_bits(data, 4);      // Only take 1111

        REQUIRE(bb.size() == 4);

        std::uint8_t output[1] = {0};
        bb.to_bytes(output, 1);

        REQUIRE(output[0] == 0xF0); // 1111 0000 (padded)
    }
}

TEST_CASE("BitBuffer append_value", "[bitbuffer]") {
    BitBuffer<128> bb;

    SECTION("append 4 bits") {
        bb.append_value(0xA, 4); // 1010

        std::uint8_t output[1] = {0};
        bb.to_bytes(output, 1);

        REQUIRE(output[0] == 0xA0); // 1010 0000
    }

    SECTION("append 8 bits") {
        bb.append_value(0xAB, 8);

        std::uint8_t output[1] = {0};
        bb.to_bytes(output, 1);

        REQUIRE(output[0] == 0xAB);
    }

    SECTION("append multiple values") {
        bb.append_value(0xA, 4); // 1010
        bb.append_value(0xB, 4); // 1011

        std::uint8_t output[1] = {0};
        bb.to_bytes(output, 1);

        REQUIRE(output[0] == 0xAB);
    }
}

TEST_CASE("BitBuffer append_bitvector", "[bitbuffer]") {
    BitBuffer<128> bb;
    BitVector<8> bv;

    SECTION("append empty bitvector bits") {
        bb.append_bitvector(bv, 8);
        REQUIRE(bb.size() == 8);

        std::uint8_t output[1] = {0};
        bb.to_bytes(output, 1);

        REQUIRE(output[0] == 0x00);
    }

    SECTION("append bitvector with some bits set") {
        bv.set_bit(0, 1); // MSB
        bv.set_bit(2, 1);
        bv.set_bit(5, 1);
        bv.set_bit(7, 1); // LSB
        // Pattern: 1010 0101 = 0xA5

        bb.append_bitvector(bv, 8);

        std::uint8_t output[1] = {0};
        bb.to_bytes(output, 1);

        REQUIRE(output[0] == 0xA5);
    }
}

TEST_CASE("BitBuffer clear", "[bitbuffer]") {
    BitBuffer<64> bb;

    bb.append_value(0xFF, 8);
    REQUIRE(bb.size() == 8);

    bb.clear();
    REQUIRE(bb.size() == 0);
}

TEST_CASE("BitBuffer multi-byte operations", "[bitbuffer]") {
    BitBuffer<256> bb;

    SECTION("cross byte boundary") {
        bb.append_value(0x0F, 4); // 0000 1111 -> 1111
        bb.append_value(0xAB, 8); // 1010 1011
        bb.append_value(0x03, 4); // 0000 0011 -> 0011

        // Total: 1111 1010 1011 0011 = 0xFA 0xB3

        std::uint8_t output[2] = {0};
        bb.to_bytes(output, 2);

        REQUIRE(output[0] == 0xFA);
        REQUIRE(output[1] == 0xB3);
    }
}

TEST_CASE("BitBuffer unaligned to_bytes", "[bitbuffer]") {
    BitBuffer<64> bb;

    bb.append_value(0x0F, 4); // 1111
    REQUIRE(bb.size() == 4);

    std::uint8_t output[1] = {0};
    std::size_t bytes = bb.to_bytes(output, 1);

    REQUIRE(bytes == 1);
    REQUIRE(output[0] == 0xF0); // 1111 0000 (padded)
}

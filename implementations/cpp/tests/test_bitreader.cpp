/**
 * @file test_bitreader.cpp
 * @brief Unit tests for BitReader class.
 */

#include <ccsds124/bitreader.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace ccsds124;

TEST_CASE("BitReader construction", "[bitreader]") {
    std::uint8_t data[] = {0xAB, 0xCD};

    SECTION("with byte array") {
        BitReader reader(data, 16);
        REQUIRE(reader.remaining() == 16);
        REQUIRE(reader.position() == 0);
    }
}

TEST_CASE("BitReader read_bit", "[bitreader]") {
    std::uint8_t data[] = {0xA5}; // 1010 0101

    BitReader reader(data, 8);

    SECTION("read individual bits MSB-first") {
        REQUIRE(reader.read_bit() == 1);
        REQUIRE(reader.read_bit() == 0);
        REQUIRE(reader.read_bit() == 1);
        REQUIRE(reader.read_bit() == 0);
        REQUIRE(reader.read_bit() == 0);
        REQUIRE(reader.read_bit() == 1);
        REQUIRE(reader.read_bit() == 0);
        REQUIRE(reader.read_bit() == 1);
    }

    SECTION("read past end returns -1") {
        for (int i = 0; i < 8; ++i) {
            reader.read_bit();
        }
        REQUIRE(reader.read_bit() == -1);
    }
}

TEST_CASE("BitReader read_bits", "[bitreader]") {
    std::uint8_t data[] = {0xAB, 0xCD};

    SECTION("read 4 bits") {
        BitReader reader(data, 16);
        REQUIRE(reader.read_bits(4) == 0xA);
        REQUIRE(reader.read_bits(4) == 0xB);
    }

    SECTION("read 8 bits") {
        BitReader reader(data, 16);
        REQUIRE(reader.read_bits(8) == 0xAB);
        REQUIRE(reader.read_bits(8) == 0xCD);
    }

    SECTION("read 16 bits") {
        BitReader reader(data, 16);
        REQUIRE(reader.read_bits(16) == 0xABCD);
    }

    SECTION("read unaligned") {
        BitReader reader(data, 16);
        REQUIRE(reader.read_bits(4) == 0xA);  // 1010
        REQUIRE(reader.read_bits(8) == 0xBC); // 1011 1100
        REQUIRE(reader.read_bits(4) == 0xD);  // 1101
    }
}

TEST_CASE("BitReader position and remaining", "[bitreader]") {
    std::uint8_t data[] = {0xFF, 0xFF};
    BitReader reader(data, 16);

    REQUIRE(reader.position() == 0);
    REQUIRE(reader.remaining() == 16);

    reader.read_bits(5);
    REQUIRE(reader.position() == 5);
    REQUIRE(reader.remaining() == 11);

    reader.read_bit();
    REQUIRE(reader.position() == 6);
    REQUIRE(reader.remaining() == 10);
}

TEST_CASE("BitReader align_byte", "[bitreader]") {
    std::uint8_t data[] = {0xFF, 0xAB};
    BitReader reader(data, 16);

    SECTION("align when already aligned") {
        reader.align_byte();
        REQUIRE(reader.position() == 0);
    }

    SECTION("align after reading some bits") {
        reader.read_bits(3);
        REQUIRE(reader.position() == 3);

        reader.align_byte();
        REQUIRE(reader.position() == 8);

        REQUIRE(reader.read_bits(8) == 0xAB);
    }

    SECTION("align at byte boundary") {
        reader.read_bits(8);
        reader.align_byte();
        REQUIRE(reader.position() == 8);
    }
}

TEST_CASE("BitReader unaligned bit count", "[bitreader]") {
    std::uint8_t data[] = {0xFF, 0x00};
    BitReader reader(data, 13); // Only 13 bits valid

    REQUIRE(reader.remaining() == 13);

    reader.read_bits(5);
    REQUIRE(reader.remaining() == 8);

    reader.read_bits(8);
    REQUIRE(reader.remaining() == 0);
}

TEST_CASE("BitReader 32-bit read", "[bitreader]") {
    std::uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    BitReader reader(data, 32);

    REQUIRE(reader.read_bits(32) == 0xDEADBEEF);
}

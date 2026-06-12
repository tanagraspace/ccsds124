/**
 * @file test_bitvector.cpp
 * @brief Unit tests for BitVector class.
 */

#include <ccsds124/bitvector.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace ccsds124;

TEST_CASE("BitVector construction", "[bitvector]") {
    SECTION("default construction with size") {
        BitVector<32> bv;
        REQUIRE(bv.length() == 32);
        REQUIRE(bv.num_words() == 1);
    }

    SECTION("720-bit vector (90 bytes)") {
        BitVector<720> bv;
        REQUIRE(bv.length() == 720);
        REQUIRE(bv.num_words() == 23); // ceil(720/32) = 23
    }

    SECTION("initial state is zero") {
        BitVector<64> bv;
        for (std::size_t i = 0; i < 64; ++i) {
            REQUIRE(bv.get_bit(i) == 0);
        }
    }
}

TEST_CASE("BitVector bit access", "[bitvector]") {
    BitVector<32> bv;

    SECTION("set and get individual bits") {
        bv.set_bit(0, 1);
        REQUIRE(bv.get_bit(0) == 1);

        bv.set_bit(31, 1);
        REQUIRE(bv.get_bit(31) == 1);

        bv.set_bit(15, 1);
        REQUIRE(bv.get_bit(15) == 1);
    }

    SECTION("clear bits") {
        bv.set_bit(5, 1);
        REQUIRE(bv.get_bit(5) == 1);
        bv.set_bit(5, 0);
        REQUIRE(bv.get_bit(5) == 0);
    }

    SECTION("multi-word bit access") {
        BitVector<64> bv64;
        bv64.set_bit(0, 1);
        bv64.set_bit(32, 1);
        bv64.set_bit(63, 1);
        REQUIRE(bv64.get_bit(0) == 1);
        REQUIRE(bv64.get_bit(32) == 1);
        REQUIRE(bv64.get_bit(63) == 1);
        REQUIRE(bv64.get_bit(1) == 0);
        REQUIRE(bv64.get_bit(31) == 0);
    }
}

TEST_CASE("BitVector zero", "[bitvector]") {
    BitVector<32> bv;
    bv.set_bit(0, 1);
    bv.set_bit(15, 1);
    bv.set_bit(31, 1);

    bv.zero();

    for (std::size_t i = 0; i < 32; ++i) {
        REQUIRE(bv.get_bit(i) == 0);
    }
}

TEST_CASE("BitVector copy", "[bitvector]") {
    BitVector<32> src;
    src.set_bit(5, 1);
    src.set_bit(20, 1);

    BitVector<32> dest;
    dest.copy_from(src);

    REQUIRE(dest.get_bit(5) == 1);
    REQUIRE(dest.get_bit(20) == 1);
    REQUIRE(dest.get_bit(0) == 0);
}

TEST_CASE("BitVector XOR", "[bitvector]") {
    BitVector<32> a, b, result;

    a.set_bit(0, 1);
    a.set_bit(1, 1);
    b.set_bit(1, 1);
    b.set_bit(2, 1);

    result.xor_of(a, b);

    REQUIRE(result.get_bit(0) == 1); // 1 XOR 0 = 1
    REQUIRE(result.get_bit(1) == 0); // 1 XOR 1 = 0
    REQUIRE(result.get_bit(2) == 1); // 0 XOR 1 = 1
    REQUIRE(result.get_bit(3) == 0); // 0 XOR 0 = 0
}

TEST_CASE("BitVector OR", "[bitvector]") {
    BitVector<32> a, b, result;

    a.set_bit(0, 1);
    a.set_bit(1, 1);
    b.set_bit(1, 1);
    b.set_bit(2, 1);

    result.or_of(a, b);

    REQUIRE(result.get_bit(0) == 1); // 1 OR 0 = 1
    REQUIRE(result.get_bit(1) == 1); // 1 OR 1 = 1
    REQUIRE(result.get_bit(2) == 1); // 0 OR 1 = 1
    REQUIRE(result.get_bit(3) == 0); // 0 OR 0 = 0
}

TEST_CASE("BitVector AND", "[bitvector]") {
    BitVector<32> a, b, result;

    a.set_bit(0, 1);
    a.set_bit(1, 1);
    b.set_bit(1, 1);
    b.set_bit(2, 1);

    result.and_of(a, b);

    REQUIRE(result.get_bit(0) == 0); // 1 AND 0 = 0
    REQUIRE(result.get_bit(1) == 1); // 1 AND 1 = 1
    REQUIRE(result.get_bit(2) == 0); // 0 AND 1 = 0
    REQUIRE(result.get_bit(3) == 0); // 0 AND 0 = 0
}

TEST_CASE("BitVector NOT", "[bitvector]") {
    BitVector<8> a, result;
    a.set_bit(0, 1);
    a.set_bit(2, 1);

    result.not_of(a);

    REQUIRE(result.get_bit(0) == 0);
    REQUIRE(result.get_bit(1) == 1);
    REQUIRE(result.get_bit(2) == 0);
    REQUIRE(result.get_bit(3) == 1);
    REQUIRE(result.get_bit(7) == 1);
}

TEST_CASE("BitVector left shift", "[bitvector]") {
    BitVector<8> a, result;
    a.set_bit(0, 1); // MSB
    a.set_bit(7, 1); // LSB

    result.left_shift_of(a);

    // Bit 0 shifted out, bit 7 moves to 6, 0 inserted at bit 7
    REQUIRE(result.get_bit(0) == 0); // Shifted out
    REQUIRE(result.get_bit(1) == 0);
    REQUIRE(result.get_bit(6) == 1); // Was at bit 7
    REQUIRE(result.get_bit(7) == 0); // New LSB is 0
}

TEST_CASE("BitVector reverse", "[bitvector]") {
    BitVector<8> a, result;
    a.set_bit(0, 1); // MSB

    result.reverse_of(a);

    REQUIRE(result.get_bit(0) == 0);
    REQUIRE(result.get_bit(7) == 1); // MSB moved to LSB
}

TEST_CASE("BitVector hamming weight", "[bitvector]") {
    BitVector<32> bv;
    REQUIRE(bv.hamming_weight() == 0);

    bv.set_bit(0, 1);
    REQUIRE(bv.hamming_weight() == 1);

    bv.set_bit(15, 1);
    bv.set_bit(31, 1);
    REQUIRE(bv.hamming_weight() == 3);
}

TEST_CASE("BitVector equality", "[bitvector]") {
    BitVector<32> a, b;

    SECTION("empty vectors are equal") {
        REQUIRE(a == b);
    }

    SECTION("same bits are equal") {
        a.set_bit(5, 1);
        b.set_bit(5, 1);
        REQUIRE(a == b);
    }

    SECTION("different bits are not equal") {
        a.set_bit(5, 1);
        b.set_bit(6, 1);
        REQUIRE(a != b);
    }
}

TEST_CASE("BitVector from_bytes", "[bitvector]") {
    SECTION("single byte") {
        BitVector<8> bv;
        std::uint8_t data[] = {0xA5}; // 1010 0101
        bv.from_bytes(data, 1);

        REQUIRE(bv.get_bit(0) == 1); // MSB
        REQUIRE(bv.get_bit(1) == 0);
        REQUIRE(bv.get_bit(2) == 1);
        REQUIRE(bv.get_bit(3) == 0);
        REQUIRE(bv.get_bit(4) == 0);
        REQUIRE(bv.get_bit(5) == 1);
        REQUIRE(bv.get_bit(6) == 0);
        REQUIRE(bv.get_bit(7) == 1); // LSB
    }

    SECTION("multiple bytes") {
        BitVector<16> bv;
        std::uint8_t data[] = {0xAB, 0xCD};
        bv.from_bytes(data, 2);

        // Check first byte (0xAB = 1010 1011)
        REQUIRE(bv.get_bit(0) == 1);
        REQUIRE(bv.get_bit(1) == 0);
        REQUIRE(bv.get_bit(7) == 1);

        // Check second byte (0xCD = 1100 1101)
        REQUIRE(bv.get_bit(8) == 1);
        REQUIRE(bv.get_bit(9) == 1);
    }
}

TEST_CASE("BitVector to_bytes", "[bitvector]") {
    SECTION("single byte") {
        BitVector<8> bv;
        bv.set_bit(0, 1); // 0x80
        bv.set_bit(2, 1); // 0x20
        bv.set_bit(5, 1); // 0x04
        bv.set_bit(7, 1); // 0x01
        // Expected: 1010 0101 = 0xA5

        std::uint8_t data[1] = {0};
        bv.to_bytes(data, 1);

        REQUIRE(data[0] == 0xA5);
    }

    SECTION("round-trip") {
        BitVector<32> bv;
        std::uint8_t original[] = {0xDE, 0xAD, 0xBE, 0xEF};
        bv.from_bytes(original, 4);

        std::uint8_t output[4] = {0};
        bv.to_bytes(output, 4);

        REQUIRE(output[0] == 0xDE);
        REQUIRE(output[1] == 0xAD);
        REQUIRE(output[2] == 0xBE);
        REQUIRE(output[3] == 0xEF);
    }
}

TEST_CASE("BitVector in-place operations", "[bitvector]") {
    SECTION("xor_with") {
        BitVector<32> a, b;
        a.set_bit(0, 1);
        a.set_bit(1, 1);
        b.set_bit(1, 1);

        a.xor_with(b);

        REQUIRE(a.get_bit(0) == 1);
        REQUIRE(a.get_bit(1) == 0);
    }

    SECTION("or_with") {
        BitVector<32> a, b;
        a.set_bit(0, 1);
        b.set_bit(1, 1);

        a.or_with(b);

        REQUIRE(a.get_bit(0) == 1);
        REQUIRE(a.get_bit(1) == 1);
    }

    SECTION("and_with") {
        BitVector<32> a, b;
        a.set_bit(0, 1);
        a.set_bit(1, 1);
        b.set_bit(1, 1);

        a.and_with(b);

        REQUIRE(a.get_bit(0) == 0);
        REQUIRE(a.get_bit(1) == 1);
    }

    SECTION("invert") {
        BitVector<8> a;
        a.set_bit(0, 1);
        a.invert();

        REQUIRE(a.get_bit(0) == 0);
        REQUIRE(a.get_bit(1) == 1);
    }

    SECTION("left_shift") {
        BitVector<8> a;
        a.set_bit(1, 1);
        a.left_shift();

        REQUIRE(a.get_bit(0) == 1);
        REQUIRE(a.get_bit(1) == 0);
    }
}

TEST_CASE("BitVector 720-bit operations", "[bitvector]") {
    // Test with actual packet size (90 bytes = 720 bits)
    BitVector<720> a, b, result;

    std::uint8_t data_a[90] = {0};
    std::uint8_t data_b[90] = {0};
    data_a[0] = 0xFF;
    data_a[89] = 0xFF;
    data_b[0] = 0x0F;
    data_b[45] = 0xAA;

    a.from_bytes(data_a, 90);
    b.from_bytes(data_b, 90);

    result.xor_of(a, b);

    // First byte: 0xFF XOR 0x0F = 0xF0
    std::uint8_t output[90] = {0};
    result.to_bytes(output, 90);
    REQUIRE(output[0] == 0xF0);
    REQUIRE(output[45] == 0xAA); // Only in b
    REQUIRE(output[89] == 0xFF); // Only in a
}

/**
 * @file test_decoder.cpp
 * @brief Unit tests for Decoder functions (COUNT, RLE, bit insert).
 */

#include <ccsds124/bitbuffer.hpp>
#include <ccsds124/bitreader.hpp>
#include <ccsds124/bitvector.hpp>
#include <ccsds124/decoder.hpp>
#include <ccsds124/encoder.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace ccsds124;

TEST_CASE("COUNT decode A=1", "[decoder]") {
    // '0' encodes A=1
    std::uint8_t data[] = {0x00}; // '0' followed by padding
    BitReader reader(data, 1);

    std::uint32_t value = 0;
    auto result = count_decode(reader, value);
    REQUIRE(result == Error::Ok);
    REQUIRE(value == 1);
    REQUIRE(reader.position() == 1);
}

TEST_CASE("COUNT decode terminator", "[decoder]") {
    // '10' encodes terminator (value=0)
    std::uint8_t data[] = {0x80}; // '10' followed by padding
    BitReader reader(data, 2);

    std::uint32_t value = 99;
    auto result = count_decode(reader, value);
    REQUIRE(result == Error::Ok);
    REQUIRE(value == 0);
    REQUIRE(reader.position() == 2);
}

TEST_CASE("COUNT decode A=2-33", "[decoder]") {
    SECTION("A=2") {
        // '110' + BIT5(0) = '11000000' = 0xC0
        std::uint8_t data[] = {0xC0};
        BitReader reader(data, 8);

        std::uint32_t value = 0;
        auto result = count_decode(reader, value);
        REQUIRE(result == Error::Ok);
        REQUIRE(value == 2);
    }

    SECTION("A=10") {
        // '110' + BIT5(8) = '11001000' = 0xC8
        std::uint8_t data[] = {0xC8};
        BitReader reader(data, 8);

        std::uint32_t value = 0;
        auto result = count_decode(reader, value);
        REQUIRE(result == Error::Ok);
        REQUIRE(value == 10);
    }

    SECTION("A=33") {
        // '110' + BIT5(31) = '11011111' = 0xDF
        std::uint8_t data[] = {0xDF};
        BitReader reader(data, 8);

        std::uint32_t value = 0;
        auto result = count_decode(reader, value);
        REQUIRE(result == Error::Ok);
        REQUIRE(value == 33);
    }
}

TEST_CASE("COUNT decode A>=34", "[decoder]") {
    // Encode A=34 and decode it
    BitBuffer<64> bb;
    auto enc_result = count_encode(bb, 34);
    REQUIRE(enc_result == Error::Ok);

    std::uint8_t encoded[8] = {0};
    bb.to_bytes(encoded, 8);

    BitReader reader(encoded, bb.size());
    std::uint32_t value = 0;
    auto result = count_decode(reader, value);
    REQUIRE(result == Error::Ok);
    REQUIRE(value == 34);
}

TEST_CASE("COUNT decode large value", "[decoder]") {
    // Encode A=1000 and decode it
    BitBuffer<64> bb;
    auto enc_result = count_encode(bb, 1000);
    REQUIRE(enc_result == Error::Ok);

    std::uint8_t encoded[8] = {0};
    bb.to_bytes(encoded, 8);

    BitReader reader(encoded, bb.size());
    std::uint32_t value = 0;
    auto result = count_decode(reader, value);
    REQUIRE(result == Error::Ok);
    REQUIRE(value == 1000);
}

TEST_CASE("COUNT decode underflow", "[decoder]") {
    std::uint8_t data[] = {0xFF};
    BitReader reader(data, 0); // No bits available

    std::uint32_t value = 0;
    auto result = count_decode(reader, value);
    REQUIRE(result == Error::Underflow);
}

TEST_CASE("RLE decode all zeros", "[decoder]") {
    // RLE of all zeros = just terminator '10'
    std::uint8_t data[] = {0x80}; // '10'
    BitReader reader(data, 2);

    BitVector<8> result;
    auto status = rle_decode(reader, result);
    REQUIRE(status == Error::Ok);
    REQUIRE(result.hamming_weight() == 0);
}

TEST_CASE("RLE decode single bit", "[decoder]") {
    // Encode a single bit at position 0 (MSB)
    BitVector<8> original;
    original.set_bit(0, 1);

    BitBuffer<64> bb;
    auto enc_result = rle_encode(bb, original);
    REQUIRE(enc_result == Error::Ok);

    std::uint8_t encoded[8] = {0};
    bb.to_bytes(encoded, 8);

    // Decode
    BitReader reader(encoded, bb.size());
    BitVector<8> decoded;
    auto dec_result = rle_decode(reader, decoded);
    REQUIRE(dec_result == Error::Ok);
    REQUIRE(decoded == original);
}

TEST_CASE("RLE decode multiple bits", "[decoder]") {
    // Encode multiple bits
    BitVector<8> original;
    original.set_bit(0, 1);
    original.set_bit(7, 1);

    BitBuffer<64> bb;
    auto enc_result = rle_encode(bb, original);
    REQUIRE(enc_result == Error::Ok);

    std::uint8_t encoded[8] = {0};
    bb.to_bytes(encoded, 8);

    // Decode
    BitReader reader(encoded, bb.size());
    BitVector<8> decoded;
    auto dec_result = rle_decode(reader, decoded);
    REQUIRE(dec_result == Error::Ok);
    REQUIRE(decoded == original);
}

TEST_CASE("RLE encode/decode round trip", "[decoder]") {
    // Test various patterns
    SECTION("alternating bits") {
        BitVector<16> original;
        for (std::size_t i = 0; i < 16; i += 2) {
            original.set_bit(i, 1);
        }

        BitBuffer<128> bb;
        auto enc_result = rle_encode(bb, original);
        REQUIRE(enc_result == Error::Ok);

        std::uint8_t encoded[16] = {0};
        bb.to_bytes(encoded, 16);

        BitReader reader(encoded, bb.size());
        BitVector<16> decoded;
        auto dec_result = rle_decode(reader, decoded);
        REQUIRE(dec_result == Error::Ok);
        REQUIRE(decoded == original);
    }

    SECTION("all ones") {
        BitVector<8> original;
        for (std::size_t i = 0; i < 8; ++i) {
            original.set_bit(i, 1);
        }

        BitBuffer<128> bb;
        auto enc_result = rle_encode(bb, original);
        REQUIRE(enc_result == Error::Ok);

        std::uint8_t encoded[16] = {0};
        bb.to_bytes(encoded, 16);

        BitReader reader(encoded, bb.size());
        BitVector<8> decoded;
        auto dec_result = rle_decode(reader, decoded);
        REQUIRE(dec_result == Error::Ok);
        REQUIRE(decoded == original);
    }

    SECTION("sparse bits") {
        BitVector<32> original;
        original.set_bit(0, 1);
        original.set_bit(15, 1);
        original.set_bit(31, 1);

        BitBuffer<128> bb;
        auto enc_result = rle_encode(bb, original);
        REQUIRE(enc_result == Error::Ok);

        std::uint8_t encoded[16] = {0};
        bb.to_bytes(encoded, 16);

        BitReader reader(encoded, bb.size());
        BitVector<32> decoded;
        auto dec_result = rle_decode(reader, decoded);
        REQUIRE(dec_result == Error::Ok);
        REQUIRE(decoded == original);
    }
}

TEST_CASE("Bit insert reverse order", "[decoder]") {
    // Create mask and original data
    BitVector<8> data;
    BitVector<8> mask;

    // Mask indicates positions 1, 4, 6
    mask.set_bit(1, 1);
    mask.set_bit(4, 1);
    mask.set_bit(6, 1);

    // Bits to insert: '101' -> positions 6, 4, 1 (reverse order)
    std::uint8_t bits[] = {0xA0}; // '101' followed by zeros
    BitReader reader(bits, 3);

    auto result = bit_insert(reader, data, mask);
    REQUIRE(result == Error::Ok);

    // Check that bits were inserted at correct positions
    // Inserted in reverse: position 6 gets first bit (1), position 4 gets second (0), position 1
    // gets third (1)
    REQUIRE(data.get_bit(6) == 1);
    REQUIRE(data.get_bit(4) == 0);
    REQUIRE(data.get_bit(1) == 1);
}

TEST_CASE("Bit insert forward order", "[decoder]") {
    BitVector<8> data;
    BitVector<8> mask;

    mask.set_bit(0, 1);
    mask.set_bit(7, 1);

    // Bits to insert: '11' -> positions 0, 7 (forward order)
    std::uint8_t bits[] = {0xC0}; // '11' followed by zeros
    BitReader reader(bits, 2);

    auto result = bit_insert_forward(reader, data, mask);
    REQUIRE(result == Error::Ok);

    REQUIRE(data.get_bit(0) == 1);
    REQUIRE(data.get_bit(7) == 1);
}

TEST_CASE("Bit extract/insert round trip", "[decoder]") {
    // Original data
    BitVector<8> original;
    original.set_bit(0, 1);
    original.set_bit(2, 1);
    original.set_bit(4, 0);
    original.set_bit(6, 1);

    // Mask
    BitVector<8> mask;
    mask.set_bit(0, 1);
    mask.set_bit(2, 1);
    mask.set_bit(4, 1);
    mask.set_bit(6, 1);

    // Extract
    BitBuffer<64> bb;
    auto ext_result = bit_extract(bb, original, mask);
    REQUIRE(ext_result == Error::Ok);

    std::uint8_t extracted[8] = {0};
    bb.to_bytes(extracted, 8);

    // Insert back
    BitReader reader(extracted, bb.size());
    BitVector<8> reconstructed;
    auto ins_result = bit_insert(reader, reconstructed, mask);
    REQUIRE(ins_result == Error::Ok);

    // Verify only masked bits match
    for (std::size_t i = 0; i < 8; ++i) {
        if (mask.get_bit(i) != 0) {
            REQUIRE(reconstructed.get_bit(i) == original.get_bit(i));
        }
    }
}

TEST_CASE("Bit insert with empty mask", "[decoder]") {
    BitVector<8> data;
    BitVector<8> mask; // All zeros

    std::uint8_t bits[] = {0xFF};
    BitReader reader(bits, 8);

    auto result = bit_insert(reader, data, mask);
    REQUIRE(result == Error::Ok);
    REQUIRE(data.hamming_weight() == 0); // No bits inserted
    REQUIRE(reader.position() == 0);     // No bits consumed
}

TEST_CASE("RLE decode rejects invalid delta", "[decoder][hardening]") {
    // GOTCHAS #21 / issue #92: an RLE delta exceeding the remaining bit
    // position must be rejected, not silently skipped.
    // COUNT(9) = '110'+BIT5(7), then terminator '10' -> 0xC7 0x80.
    std::uint8_t data[] = {0xC7, 0x80};
    BitReader reader(data, 16);

    BitVector<8> result;
    auto status = rle_decode(reader, result);
    REQUIRE(status != Error::Ok);
}

TEST_CASE("Bit insert rejects truncated input", "[decoder][hardening]") {
    // GOTCHAS #21 / issue #92: bit_insert must fail on underflow instead of
    // silently skipping positions. Mask needs 8 bits but only 3 available.
    std::uint8_t data[] = {0xE0};
    BitReader reader(data, 3);

    BitVector<8> mask;
    for (std::size_t i = 0; i < 8; ++i) {
        mask.set_bit(i, 1);
    }
    BitVector<8> out;
    REQUIRE(bit_insert(reader, out, mask) != Error::Ok);
}

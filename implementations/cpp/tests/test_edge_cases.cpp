/**
 * @file test_edge_cases.cpp
 * @brief Comprehensive edge case tests for CCSDS 124.0-B-1 implementation.
 *
 * Tests boundary conditions, corner cases, and stress scenarios.
 */

#include <ccsds124/bitbuffer.hpp>
#include <ccsds124/bitreader.hpp>
#include <ccsds124/bitvector.hpp>
#include <ccsds124/ccsds124.hpp>
#include <ccsds124/compressor.hpp>
#include <ccsds124/decoder.hpp>
#include <ccsds124/decompressor.hpp>
#include <ccsds124/encoder.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace ccsds124;

// ============================================================================
// BitVector Edge Cases
// ============================================================================

TEST_CASE("BitVector edge cases", "[edge][bitvector]") {
    SECTION("minimum size (1 bit)") {
        BitVector<1> bv;
        bv.set_bit(0, 1);
        REQUIRE(bv.get_bit(0) == 1);
        REQUIRE(bv.hamming_weight() == 1);
    }

    SECTION("word boundary (32 bits)") {
        BitVector<32> bv;
        // Set first and last bit
        bv.set_bit(0, 1);
        bv.set_bit(31, 1);
        REQUIRE(bv.hamming_weight() == 2);

        // Reverse using reverse_of
        BitVector<32> reversed;
        reversed.reverse_of(bv);
        REQUIRE(reversed.get_bit(0) == 1);
        REQUIRE(reversed.get_bit(31) == 1);
    }

    SECTION("cross-word boundary (33 bits)") {
        BitVector<33> bv;
        // Set bit in second word
        bv.set_bit(32, 1);
        REQUIRE(bv.get_bit(32) == 1);
        REQUIRE(bv.hamming_weight() == 1);
    }

    SECTION("all bits set") {
        BitVector<64> bv;
        for (std::size_t i = 0; i < 64; ++i) {
            bv.set_bit(i, 1);
        }
        REQUIRE(bv.hamming_weight() == 64);

        // NOT using not_of
        BitVector<64> notted;
        notted.not_of(bv);
        REQUIRE(notted.hamming_weight() == 0);
    }

    SECTION("XOR with self gives zero") {
        BitVector<32> bv;
        bv.set_bit(0, 1);
        bv.set_bit(15, 1);
        bv.set_bit(31, 1);

        BitVector<32> result;
        result.xor_of(bv, bv);
        REQUIRE(result.hamming_weight() == 0);
    }

    SECTION("left shift edge cases") {
        BitVector<8> bv;
        bv.set_bit(7, 1); // LSB

        // left_shift_of shifts left by 1 position
        BitVector<8> shifted;
        shifted.left_shift_of(bv);
        // Bit at position 7 moves to position 6
        REQUIRE(shifted.get_bit(6) == 1);
        REQUIRE(shifted.hamming_weight() == 1);
    }

    SECTION("from_bytes and to_bytes round-trip") {
        BitVector<720> bv;
        std::uint8_t original[90];

        // Fill with pattern
        for (int i = 0; i < 90; ++i) {
            original[i] = static_cast<std::uint8_t>(i);
        }

        bv.from_bytes(original, 90);

        std::uint8_t result[90];
        bv.to_bytes(result, 90);

        for (int i = 0; i < 90; ++i) {
            REQUIRE(result[i] == original[i]);
        }
    }

    SECTION("non-byte-aligned size") {
        BitVector<13> bv; // 13 bits, not byte-aligned
        bv.set_bit(0, 1);
        bv.set_bit(12, 1);

        std::uint8_t bytes[2];
        bv.to_bytes(bytes, 2);

        BitVector<13> bv2;
        bv2.from_bytes(bytes, 2);
        // Only first 13 bits should match
        REQUIRE(bv2.get_bit(0) == 1);
        REQUIRE(bv2.get_bit(12) == 1);
    }
}

// ============================================================================
// BitBuffer Edge Cases
// ============================================================================

TEST_CASE("BitBuffer edge cases", "[edge][bitbuffer]") {
    SECTION("append single bits to capacity") {
        BitBuffer<8> buf;
        for (int i = 0; i < 8; ++i) {
            buf.append_bit(i % 2);
        }
        REQUIRE(buf.size() == 8);
    }

    SECTION("append value at word boundary") {
        BitBuffer<64> buf;
        // Append 8-bit value
        buf.append_value(0xAB, 8);
        REQUIRE(buf.size() == 8);

        // Append another value
        buf.append_value(0xCD, 8);
        REQUIRE(buf.size() == 16);
    }

    SECTION("append large bitvector") {
        BitBuffer<1024> buf;
        BitVector<720> vec;

        // Set alternating pattern
        for (std::size_t i = 0; i < 720; i += 2) {
            vec.set_bit(i, 1);
        }

        buf.append_bitvector(vec);
        REQUIRE(buf.size() == 720);
    }

    SECTION("to_bytes with padding") {
        BitBuffer<64> buf;
        buf.append_value(0x1F, 5); // 5 bits = needs padding

        std::uint8_t bytes[8];
        std::size_t size = buf.to_bytes(bytes, 8);
        REQUIRE(size == 1); // 5 bits = 1 byte with padding
    }
}

// ============================================================================
// BitReader Edge Cases
// ============================================================================

TEST_CASE("BitReader edge cases", "[edge][bitreader]") {
    SECTION("read exact size") {
        std::uint8_t data[] = {0xFF, 0x00};
        BitReader reader(data, 16);

        REQUIRE(reader.read_bits(8) == 0xFF);
        REQUIRE(reader.read_bits(8) == 0x00);
        REQUIRE(reader.remaining() == 0);
    }

    SECTION("align to byte boundary") {
        std::uint8_t data[] = {0xFF, 0xAA};
        BitReader reader(data, 16);

        reader.read_bits(3); // Read 3 bits
        REQUIRE(reader.position() == 3);

        reader.align_byte(); // Should skip to bit 8
        REQUIRE(reader.position() == 8);
    }

    SECTION("read spanning multiple words") {
        std::uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};
        BitReader reader(data, 64);

        std::uint32_t value = reader.read_bits(32);
        REQUIRE(value == 0xFFFFFFFF);
    }
}

// ============================================================================
// Encoder Edge Cases
// ============================================================================

TEST_CASE("COUNT encoding edge cases", "[edge][encoder]") {
    SECTION("COUNT(1) - minimum") {
        BitBuffer<64> buf;
        count_encode(buf, 1);
        // COUNT(1) = 0, 1 bit
        REQUIRE(buf.size() == 1);
    }

    SECTION("COUNT(33) - last lookup table entry") {
        BitBuffer<64> buf;
        count_encode(buf, 33);
        // COUNT(33) uses lookup table
        REQUIRE(buf.size() > 0);
    }

    SECTION("COUNT(34) - first recursive") {
        BitBuffer<64> buf;
        count_encode(buf, 34);
        REQUIRE(buf.size() > 0);
    }

    SECTION("COUNT large value") {
        BitBuffer<128> buf;
        count_encode(buf, 1000);
        REQUIRE(buf.size() > 0);
    }
}

TEST_CASE("RLE encoding edge cases", "[edge][encoder]") {
    SECTION("all zeros (no runs)") {
        BitBuffer<64> buf;
        BitVector<32> vec; // All zeros
        rle_encode(buf, vec);
        // Should be just the terminator '10' (2 bits)
        REQUIRE(buf.size() == 2);
    }

    SECTION("single bit at MSB") {
        BitBuffer<64> buf;
        BitVector<32> vec;
        vec.set_bit(0, 1);
        rle_encode(buf, vec);
        // Should have encoded data + terminator
        REQUIRE(buf.size() > 0);
    }

    SECTION("single bit at LSB") {
        BitBuffer<64> buf;
        BitVector<32> vec;
        vec.set_bit(31, 1);
        rle_encode(buf, vec);
        // COUNT(32) + terminator
        REQUIRE(buf.size() > 1);
    }

    SECTION("alternating bits") {
        BitBuffer<256> buf;
        BitVector<32> vec;
        for (std::size_t i = 0; i < 32; i += 2) {
            vec.set_bit(i, 1);
        }
        rle_encode(buf, vec);
        // 16 runs of length 1, each needs COUNT(1)=0 and COUNT(1)=0
        REQUIRE(buf.size() > 0);
    }
}

TEST_CASE("BE (bit extraction) edge cases", "[edge][encoder]") {
    SECTION("all bits masked out") {
        BitBuffer<64> buf;
        BitVector<8> input;
        BitVector<8> mask; // All zeros = all masked out

        input.set_bit(0, 1);
        input.set_bit(7, 1);

        bit_extract(buf, input, mask);
        // No bits to extract
        REQUIRE(buf.size() == 0);
    }

    SECTION("all bits unmasked") {
        BitBuffer<64> buf;
        BitVector<8> input;
        BitVector<8> mask;

        // Set input
        input.set_bit(0, 1);
        input.set_bit(7, 1);

        // All unmasked
        for (std::size_t i = 0; i < 8; ++i) {
            mask.set_bit(i, 1);
        }

        bit_extract(buf, input, mask);
        REQUIRE(buf.size() == 8); // All 8 bits extracted
    }
}

// ============================================================================
// Decoder Edge Cases
// ============================================================================

TEST_CASE("COUNT decode edge cases", "[edge][decoder]") {
    SECTION("COUNT(1) decode") {
        std::uint8_t data[] = {0x00}; // 0 = COUNT(1)
        BitReader reader(data, 1);
        std::uint32_t value = 0;
        auto result = count_decode(reader, value);
        REQUIRE(result == Error::Ok);
        REQUIRE(value == 1);
    }

    SECTION("Terminator decode") {
        std::uint8_t data[] = {0x80}; // 10 = terminator (returns 0)
        BitReader reader(data, 2);
        std::uint32_t value = 99;
        auto result = count_decode(reader, value);
        REQUIRE(result == Error::Ok);
        REQUIRE(value == 0);
    }
}

TEST_CASE("RLE decode edge cases", "[edge][decoder]") {
    SECTION("empty input (terminator only)") {
        std::uint8_t data[] = {0x80}; // 10 = terminator
        BitReader reader(data, 2);
        BitVector<8> result;
        auto status = rle_decode(reader, result);
        REQUIRE(status == Error::Ok);
        // All zeros
        REQUIRE(result.hamming_weight() == 0);
    }
}

// ============================================================================
// Compressor/Decompressor Edge Cases
// ============================================================================

TEST_CASE("Compressor edge cases", "[edge][compressor]") {
    SECTION("constant zero input") {
        Compressor<8> comp;
        BitVector<8> input; // All zeros
        BitBuffer<64> output;

        // First packet
        CompressParams params;
        params.uncompressed_flag = true;
        comp.compress_packet(input, output, &params);

        // Second packet - should compress well (predictable)
        output.clear();
        params.uncompressed_flag = false;
        comp.compress_packet(input, output, &params);
        REQUIRE(output.size() > 0);
    }

    SECTION("constant one input") {
        Compressor<8> comp;
        BitVector<8> input;
        BitBuffer<64> output;

        // All ones
        for (std::size_t i = 0; i < 8; ++i) {
            input.set_bit(i, 1);
        }

        CompressParams params;
        params.uncompressed_flag = true;
        comp.compress_packet(input, output, &params);
        REQUIRE(output.size() > 0);
    }

    SECTION("maximum robustness (R=7)") {
        Compressor<8> comp(7);
        BitVector<8> input;
        BitBuffer<256> output;

        // Need R+1 = 8 packets with uncompressed
        for (int i = 0; i < 10; ++i) {
            input.zero();
            input.set_bit(static_cast<std::size_t>(i % 8), 1);

            CompressParams params;
            if (i <= 7) {
                params.uncompressed_flag = true;
                params.send_mask_flag = true;
            }

            output.clear();
            auto result = comp.compress_packet(input, output, &params);
            REQUIRE(result == Error::Ok);
        }
    }

    SECTION("rapidly changing input") {
        Compressor<32> comp(2);
        BitVector<32> input;
        BitBuffer<512> output;

        for (int i = 0; i < 20; ++i) {
            // Completely different pattern each time
            input.zero();
            for (std::size_t j = 0; j < 32; ++j) {
                input.set_bit(j, ((i + j) % 3 == 0) ? 1 : 0);
            }

            CompressParams params;
            if (i <= 2) {
                params.uncompressed_flag = true;
                params.send_mask_flag = true;
            }

            output.clear();
            auto result = comp.compress_packet(input, output, &params);
            REQUIRE(result == Error::Ok);
        }
    }
}

TEST_CASE("Decompressor edge cases", "[edge][decompressor]") {
    SECTION("round-trip with 31-bit boundary pattern") {
        Compressor<31> comp(1);
        Decompressor<31> decomp(1);
        BitVector<31> input;
        BitBuffer<256> compressed;

        input.set_bit(0, 1);
        input.set_bit(30, 1);

        CompressParams params;
        params.uncompressed_flag = true;
        params.send_mask_flag = true;
        comp.compress_packet(input, compressed, &params);

        std::uint8_t bytes[32];
        compressed.to_bytes(bytes, 32);
        BitReader reader(bytes, compressed.size());

        BitVector<31> output;
        decomp.decompress_packet(reader, output);

        REQUIRE(output.get_bit(0) == input.get_bit(0));
        REQUIRE(output.get_bit(30) == input.get_bit(30));
    }
}

// ============================================================================
// High-Level API Edge Cases
// ============================================================================

TEST_CASE("High-level API edge cases", "[edge][api]") {
    SECTION("minimum valid input (1 packet)") {
        std::uint8_t input[90] = {0};
        input[0] = 0xFF;

        std::uint8_t output[180];
        std::size_t output_size = 0;

        auto result = compress<720>(input, 90, output, 180, output_size, 1, 10, 20, 50);
        REQUIRE(result == Error::Ok);
        REQUIRE(output_size > 0);

        // Decompress
        std::uint8_t decompressed[90];
        std::size_t decompressed_size = 0;
        result = decompress<720>(output, output_size, decompressed, 90, decompressed_size, 1);
        REQUIRE(result == Error::Ok);
        REQUIRE(decompressed_size == 90);
        REQUIRE(decompressed[0] == input[0]);
    }

    SECTION("empty input") {
        std::uint8_t output[100];
        std::size_t output_size = 0;

        auto result = compress<720>(nullptr, 0, output, 100, output_size, 1, 10, 20, 50);
        REQUIRE(result == Error::InvalidArg);
    }

    SECTION("input not multiple of packet size") {
        std::uint8_t input[100]; // 100 bytes, not divisible by 90
        std::uint8_t output[200];
        std::size_t output_size = 0;

        auto result = compress<720>(input, 100, output, 200, output_size, 1, 10, 20, 50);
        REQUIRE(result == Error::InvalidArg);
    }
}

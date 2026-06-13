/**
 * @file test_decompressor.cpp
 * @brief Unit tests for Decompressor class.
 */

#include <ccsds124/bitbuffer.hpp>
#include <ccsds124/bitreader.hpp>
#include <ccsds124/bitvector.hpp>
#include <ccsds124/compressor.hpp>
#include <ccsds124/decompressor.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace ccsds124;

TEST_CASE("Decompressor construction", "[decompressor]") {
    SECTION("default construction") {
        Decompressor<8> decomp;
        REQUIRE(decomp.time_index() == 0);
        REQUIRE(decomp.robustness() == 0);
    }

    SECTION("with robustness") {
        Decompressor<8> decomp(3);
        REQUIRE(decomp.robustness() == 3);
    }

    SECTION("robustness capped at max") {
        Decompressor<8> decomp(10); // Should be capped to 7
        REQUIRE(decomp.robustness() == 7);
    }
}

TEST_CASE("Decompressor reset", "[decompressor]") {
    Decompressor<8> decomp(2);

    // Manually advance time index by setting initial mask and resetting
    BitVector<8> mask;
    mask.set_bit(0, 1);
    decomp.set_initial_mask(mask);

    decomp.reset();
    REQUIRE(decomp.time_index() == 0);
    REQUIRE(decomp.mask().get_bit(0) == 1);
}

TEST_CASE("Compress/decompress round trip - single packet", "[decompressor]") {
    Compressor<8> comp;
    Decompressor<8> decomp;

    BitVector<8> input;
    input.set_bit(0, 1);
    input.set_bit(3, 1);
    input.set_bit(7, 1);

    // First packet must be sent uncompressed (CCSDS requirement)
    CompressParams params;
    params.uncompressed_flag = true;

    // Compress
    BitBuffer<64> compressed;
    auto comp_result = comp.compress_packet(input, compressed, &params);
    REQUIRE(comp_result == Error::Ok);

    // Decompress
    std::uint8_t compressed_bytes[8] = {0};
    compressed.to_bytes(compressed_bytes, 8);
    BitReader reader(compressed_bytes, compressed.size());

    BitVector<8> output;
    auto decomp_result = decomp.decompress_packet(reader, output);
    REQUIRE(decomp_result == Error::Ok);

    // Verify
    REQUIRE(output == input);
}

TEST_CASE("Compress/decompress round trip - multiple packets", "[decompressor]") {
    Compressor<8> comp;
    Decompressor<8> decomp;

    // First packet must be uncompressed (CCSDS requirement)
    BitVector<8> input1;
    input1.set_bit(0, 1);

    CompressParams params1;
    params1.uncompressed_flag = true;
    params1.send_mask_flag = true;

    BitBuffer<64> compressed1;
    comp.compress_packet(input1, compressed1, &params1);

    std::uint8_t bytes1[8] = {0};
    compressed1.to_bytes(bytes1, 8);
    BitReader reader1(bytes1, compressed1.size());

    BitVector<8> output1;
    decomp.decompress_packet(reader1, output1);
    REQUIRE(output1 == input1);

    // Second packet - same data (predictable)
    BitBuffer<64> compressed2;
    comp.compress_packet(input1, compressed2);

    std::uint8_t bytes2[8] = {0};
    compressed2.to_bytes(bytes2, 8);
    BitReader reader2(bytes2, compressed2.size());

    BitVector<8> output2;
    decomp.decompress_packet(reader2, output2);
    REQUIRE(output2 == input1);

    // Third packet - different data
    BitVector<8> input3;
    input3.set_bit(1, 1);
    input3.set_bit(2, 1);

    BitBuffer<64> compressed3;
    comp.compress_packet(input3, compressed3);

    std::uint8_t bytes3[8] = {0};
    compressed3.to_bytes(bytes3, 8);
    BitReader reader3(bytes3, compressed3.size());

    BitVector<8> output3;
    decomp.decompress_packet(reader3, output3);
    REQUIRE(output3 == input3);
}

TEST_CASE("Compress/decompress round trip - with params", "[decompressor]") {
    Compressor<8> comp(2);
    Decompressor<8> decomp(2);

    BitVector<8> input;
    input.set_bit(0, 1);
    input.set_bit(4, 1);

    CompressParams params;
    params.min_robustness = 2;
    params.send_mask_flag = true;
    params.uncompressed_flag = true;

    BitBuffer<256> compressed;
    comp.compress_packet(input, compressed, &params);

    std::uint8_t bytes[32] = {0};
    compressed.to_bytes(bytes, 32);
    BitReader reader(bytes, compressed.size());

    BitVector<8> output;
    auto result = decomp.decompress_packet(reader, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(output == input);
}

TEST_CASE("Compress/decompress round trip - uncompressed mode", "[decompressor]") {
    Compressor<8> comp;
    Decompressor<8> decomp;

    BitVector<8> input;
    input.set_bit(0, 1);
    input.set_bit(2, 1);
    input.set_bit(4, 1);
    input.set_bit(6, 1);

    CompressParams params;
    params.uncompressed_flag = true;

    BitBuffer<256> compressed;
    comp.compress_packet(input, compressed, &params);

    std::uint8_t bytes[32] = {0};
    compressed.to_bytes(bytes, 32);
    BitReader reader(bytes, compressed.size());

    BitVector<8> output;
    auto result = decomp.decompress_packet(reader, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(output == input);
}

TEST_CASE("Compress/decompress round trip - with mask", "[decompressor]") {
    Compressor<8> comp;
    Decompressor<8> decomp;

    BitVector<8> input;
    input.set_bit(0, 1);
    input.set_bit(7, 1);

    // First packet needs both mask and data
    CompressParams params;
    params.send_mask_flag = true;
    params.uncompressed_flag = true;

    BitBuffer<256> compressed;
    comp.compress_packet(input, compressed, &params);

    std::uint8_t bytes[32] = {0};
    compressed.to_bytes(bytes, 32);
    BitReader reader(bytes, compressed.size());

    BitVector<8> output;
    auto result = decomp.decompress_packet(reader, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(output == input);
}

TEST_CASE("Compress/decompress round trip - sequence of packets", "[decompressor]") {
    Compressor<8> comp(2);
    Decompressor<8> decomp(2);

    // Sequence of 10 packets with varying data
    for (int i = 0; i < 10; ++i) {
        BitVector<8> input;
        // Create pattern based on packet index
        input.set_bit(static_cast<std::size_t>(i % 8), 1);
        if (i > 2) {
            input.set_bit(static_cast<std::size_t>((i + 3) % 8), 1);
        }

        // First R+1 packets need uncompressed flag (CCSDS initialization)
        CompressParams params;
        if (static_cast<std::size_t>(i) <= comp.robustness()) {
            params.uncompressed_flag = true;
            params.send_mask_flag = true;
        }

        BitBuffer<128> compressed;
        auto comp_result = comp.compress_packet(input, compressed, &params);
        REQUIRE(comp_result == Error::Ok);

        std::uint8_t bytes[16] = {0};
        compressed.to_bytes(bytes, 16);
        BitReader reader(bytes, compressed.size());

        BitVector<8> output;
        auto decomp_result = decomp.decompress_packet(reader, output);
        REQUIRE(decomp_result == Error::Ok);
        REQUIRE(output == input);
    }
}

TEST_CASE("Compress/decompress round trip - all zeros", "[decompressor]") {
    Compressor<8> comp;
    Decompressor<8> decomp;

    BitVector<8> input; // All zeros

    BitBuffer<64> compressed;
    comp.compress_packet(input, compressed);

    std::uint8_t bytes[8] = {0};
    compressed.to_bytes(bytes, 8);
    BitReader reader(bytes, compressed.size());

    BitVector<8> output;
    auto result = decomp.decompress_packet(reader, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(output == input);
}

TEST_CASE("Compress/decompress round trip - all ones", "[decompressor]") {
    Compressor<8> comp;
    Decompressor<8> decomp;

    BitVector<8> input;
    for (std::size_t i = 0; i < 8; ++i) {
        input.set_bit(i, 1);
    }

    // First packet needs uncompressed flag
    CompressParams params;
    params.uncompressed_flag = true;

    BitBuffer<64> compressed;
    comp.compress_packet(input, compressed, &params);

    std::uint8_t bytes[8] = {0};
    compressed.to_bytes(bytes, 8);
    BitReader reader(bytes, compressed.size());

    BitVector<8> output;
    auto result = decomp.decompress_packet(reader, output);
    REQUIRE(result == Error::Ok);
    REQUIRE(output == input);
}

TEST_CASE("Compress/decompress round trip - larger packet", "[decompressor]") {
    Compressor<720> comp(2); // 90 bytes
    Decompressor<720> decomp(2);

    BitVector<720> input;
    // Set every 8th bit
    for (std::size_t i = 0; i < 720; i += 8) {
        input.set_bit(i, 1);
    }

    // First packet needs uncompressed flag
    CompressParams params;
    params.uncompressed_flag = true;
    params.send_mask_flag = true;

    BitBuffer<720 * 6> compressed;
    auto comp_result = comp.compress_packet(input, compressed, &params);
    REQUIRE(comp_result == Error::Ok);

    std::uint8_t bytes[720] = {0};
    compressed.to_bytes(bytes, 720);
    BitReader reader(bytes, compressed.size());

    BitVector<720> output;
    auto decomp_result = decomp.decompress_packet(reader, output);
    REQUIRE(decomp_result == Error::Ok);
    REQUIRE(output == input);
}

TEST_CASE("Compress/decompress round trip - with robustness", "[decompressor]") {
    Compressor<16> comp(3);
    Decompressor<16> decomp(3);

    // Compress and decompress several packets to build robustness history
    for (int i = 0; i < 5; ++i) {
        BitVector<16> input;
        input.set_bit(static_cast<std::size_t>(i % 16), 1);
        input.set_bit(static_cast<std::size_t>((i * 3) % 16), 1);

        // First R+1 packets need uncompressed flag (CCSDS initialization)
        CompressParams params;
        if (static_cast<std::size_t>(i) <= comp.robustness()) {
            params.uncompressed_flag = true;
            params.send_mask_flag = true;
        }

        BitBuffer<256> compressed;
        comp.compress_packet(input, compressed, &params);

        std::uint8_t bytes[32] = {0};
        compressed.to_bytes(bytes, 32);
        BitReader reader(bytes, compressed.size());

        BitVector<16> output;
        auto result = decomp.decompress_packet(reader, output);
        REQUIRE(result == Error::Ok);
        REQUIRE(output == input);
    }
}

TEST_CASE("Compress/decompress round trip - alternating patterns", "[decompressor]") {
    Compressor<8> comp;
    Decompressor<8> decomp;

    // Pattern 1: 10101010
    BitVector<8> pattern1;
    for (std::size_t i = 0; i < 8; i += 2) {
        pattern1.set_bit(i, 1);
    }

    // Pattern 2: 01010101
    BitVector<8> pattern2;
    for (std::size_t i = 1; i < 8; i += 2) {
        pattern2.set_bit(i, 1);
    }

    // Alternate between patterns
    for (int i = 0; i < 6; ++i) {
        BitVector<8>& input = (i % 2 == 0) ? pattern1 : pattern2;

        // First packet needs uncompressed flag
        CompressParams params;
        if (i == 0) {
            params.uncompressed_flag = true;
            params.send_mask_flag = true;
        }

        BitBuffer<128> compressed;
        comp.compress_packet(input, compressed, &params);

        std::uint8_t bytes[16] = {0};
        compressed.to_bytes(bytes, 16);
        BitReader reader(bytes, compressed.size());

        BitVector<8> output;
        auto result = decomp.decompress_packet(reader, output);
        REQUIRE(result == Error::Ok);
        REQUIRE(output == input);
    }
}

TEST_CASE("Decompress rejects truncated packet", "[decompressor][hardening]") {
    // GOTCHAS #20 / issue #92: a packet that ends mid-header must produce an
    // error, not silently decode garbage. Two bits: RLE terminator '10',
    // then the stream ends before BIT4(Vt).
    std::uint8_t data[] = {0x80};
    BitReader reader(data, 2);

    Decompressor<8> decomp(0);
    BitVector<8> output;
    REQUIRE(decomp.decompress_packet(reader, output) != Error::Ok);
}

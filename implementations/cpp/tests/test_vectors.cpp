/**
 * @file test_vectors.cpp
 * @brief Reference vector validation tests.
 *
 * Tests compression against reference test vectors to ensure correctness
 * and interoperability with the C implementation.
 */

#include <ccsds124/ccsds124.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

using namespace ccsds124;

// Test vector paths - check environment variable first, then use relative path
static std::string get_test_vectors_dir() {
    const char* env = std::getenv("TEST_VECTORS_DIR");
    if (env != nullptr && env[0] != '\0') {
        return env;
    }
    // Default: relative path from build directory (implementations/cpp/build/)
    // Goes up to repo root: build/ -> cpp/ -> implementations/ -> ccsds124/
    return "../../../test-vectors";
}

/**
 * @brief Read file into vector.
 */
static std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }

    return buffer;
}

/**
 * @brief DJB2 hash for verification.
 */
static std::uint32_t djb2_hash(const std::uint8_t* data, std::size_t len) {
    std::uint32_t hash = 5381U;
    for (std::size_t i = 0; i < len; ++i) {
        hash = ((hash << 5U) + hash) + data[i];
    }
    return hash;
}

/**
 * @brief Compress and verify against expected output, then round-trip verify.
 */
template <std::size_t N>
static bool compress_and_verify(const std::string& input_path, const std::string& expected_path,
                                int pt_limit, int ft_limit, int rt_limit, std::uint8_t robustness) {
    // Read input file
    auto input_data = read_file(input_path);
    if (input_data.empty()) {
        INFO("Could not read input file: " << input_path);
        return false;
    }

    // Read expected output
    auto expected_output = read_file(expected_path);
    if (expected_output.empty()) {
        INFO("Could not read expected output: " << expected_path);
        return false;
    }

    // Allocate output buffer
    std::vector<std::uint8_t> actual_output(input_data.size() * 2);
    std::size_t actual_size = 0;

    // Compress
    auto result =
        compress<N>(input_data.data(), input_data.size(), actual_output.data(),
                    actual_output.size(), actual_size, robustness, pt_limit, ft_limit, rt_limit);

    if (result != Error::Ok) {
        INFO("Compression failed with error " << static_cast<int>(result));
        return false;
    }

    INFO("Input: " << input_data.size() << " bytes -> Compressed: " << actual_size
                   << " bytes (ratio: " << (static_cast<double>(input_data.size()) / actual_size)
                   << "x)");

    // Compare with expected output
    if (actual_size != expected_output.size()) {
        INFO("Size mismatch - Expected: " << expected_output.size()
                                          << " bytes, Got: " << actual_size << " bytes");
        return false;
    }

    for (std::size_t i = 0; i < actual_size; ++i) {
        if (actual_output[i] != expected_output[i]) {
            INFO("Byte mismatch at offset " << i << " - Expected: 0x" << std::hex
                                            << static_cast<int>(expected_output[i]) << ", Got: 0x"
                                            << static_cast<int>(actual_output[i]));
            return false;
        }
    }

    // Round-trip verification: decompress and verify
    std::vector<std::uint8_t> decompressed(input_data.size() * 2);
    std::size_t decompressed_size = 0;

    result = decompress<N>(actual_output.data(), actual_size, decompressed.data(),
                           decompressed.size(), decompressed_size, robustness);

    if (result != Error::Ok) {
        INFO("Decompression failed with error " << static_cast<int>(result));
        return false;
    }

    if (decompressed_size != input_data.size()) {
        INFO("Round-trip size mismatch - Original: "
             << input_data.size() << " bytes, Decompressed: " << decompressed_size << " bytes");
        return false;
    }

    // Verify hash
    std::uint32_t original_hash = djb2_hash(input_data.data(), input_data.size());
    std::uint32_t decompressed_hash = djb2_hash(decompressed.data(), decompressed_size);

    if (original_hash != decompressed_hash) {
        INFO("Round-trip hash mismatch - Original: 0x"
             << std::hex << original_hash << ", Decompressed: 0x" << decompressed_hash);
        return false;
    }

    INFO("Round-trip verified: hash 0x" << std::hex << original_hash);
    return true;
}

TEST_CASE("Test vector: simple", "[vectors]") {
    std::string base = get_test_vectors_dir();
    std::string input_path = base + "/input/simple.bin";
    std::string expected_path = base + "/expected-output/simple.bin.pkt";

    REQUIRE(compress_and_verify<720>(input_path, expected_path, 10, 20, 50, 1));
}

TEST_CASE("Test vector: housekeeping", "[vectors]") {
    std::string base = get_test_vectors_dir();
    std::string input_path = base + "/input/housekeeping.bin";
    std::string expected_path = base + "/expected-output/housekeeping.bin.pkt";

    REQUIRE(compress_and_verify<720>(input_path, expected_path, 20, 50, 100, 2));
}

TEST_CASE("Test vector: edge-cases", "[vectors]") {
    std::string base = get_test_vectors_dir();
    std::string input_path = base + "/input/edge-cases.bin";
    std::string expected_path = base + "/expected-output/edge-cases.bin.pkt";

    REQUIRE(compress_and_verify<720>(input_path, expected_path, 10, 20, 50, 1));
}

TEST_CASE("Test vector: hiro", "[vectors]") {
    std::string base = get_test_vectors_dir();
    std::string input_path = base + "/input/hiro.bin";
    std::string expected_path = base + "/expected-output/hiro.bin.pkt";

    REQUIRE(compress_and_verify<720>(input_path, expected_path, 10, 20, 50, 7));
}

TEST_CASE("Test vector: venus-express", "[vectors]") {
    std::string base = get_test_vectors_dir();
    std::string input_path = base + "/input/venus-express.ccsds";
    std::string expected_path = base + "/expected-output/venus-express.ccsds.pkt";

    REQUIRE(compress_and_verify<720>(input_path, expected_path, 20, 50, 100, 2));
}

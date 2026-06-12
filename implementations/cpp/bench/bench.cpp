/**
 * @file bench.cpp
 * @brief Performance benchmarks for CCSDS 124.0-B-1 C++ compression.
 *
 * Measures compression throughput for regression testing during development.
 * Note: Desktop performance differs from embedded targets - use for relative
 * comparisons only.
 *
 * Usage:
 *   make bench              # Run with default 100 iterations
 *   ./build/bench 1000      # Run with custom iteration count
 */

#include <ccsds124/ccsds124.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

using namespace ccsds124;

static constexpr int DEFAULT_ITERATIONS = 100;
static constexpr std::size_t PACKET_SIZE_BITS = 720;
static constexpr std::size_t PACKET_SIZE_BYTES = PACKET_SIZE_BITS / 8;

static bool load_file(const char* path, std::vector<std::uint8_t>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    data.resize(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        return false;
    }

    return true;
}

static void bench_compress(const char* name, const char* path, int robustness, int pt, int ft,
                           int rt, int iterations) {
    std::vector<std::uint8_t> input;

    if (!load_file(path, input)) {
        std::printf("%-20s SKIP (file not found)\n", name);
        return;
    }

    std::size_t num_packets = input.size() / PACKET_SIZE_BYTES;
    std::vector<std::uint8_t> output(input.size() * 2);
    std::size_t output_size = 0;

    // Warmup run
    compress<PACKET_SIZE_BITS>(input.data(), input.size(), output.data(), output.size(),
                               output_size, static_cast<std::uint8_t>(robustness), pt, ft, rt);

    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        compress<PACKET_SIZE_BITS>(input.data(), input.size(), output.data(), output.size(),
                                   output_size, static_cast<std::uint8_t>(robustness), pt, ft, rt);
    }

    auto end = std::chrono::high_resolution_clock::now();

    double total_us = std::chrono::duration<double, std::micro>(end - start).count();
    double per_iter_us = total_us / static_cast<double>(iterations);
    double per_packet_us = per_iter_us / static_cast<double>(num_packets);
    double throughput_kbps = (static_cast<double>(input.size()) * 8.0 * 1000.0) / per_iter_us;

    std::printf("%-20s %8.2f µs/iter  %6.2f µs/pkt  %8.1f Kbps  (%zu pkts)\n", name, per_iter_us,
                per_packet_us, throughput_kbps, num_packets);
}

static void bench_decompress(const char* name, const char* path, int robustness, int pt, int ft,
                             int rt, int iterations) {
    std::vector<std::uint8_t> input;

    if (!load_file(path, input)) {
        std::printf("%-20s SKIP (file not found)\n", name);
        return;
    }

    std::size_t num_packets = input.size() / PACKET_SIZE_BYTES;

    // First compress the data
    std::vector<std::uint8_t> compressed(input.size() * 2);
    std::size_t compressed_size = 0;
    compress<PACKET_SIZE_BITS>(input.data(), input.size(), compressed.data(), compressed.size(),
                               compressed_size, static_cast<std::uint8_t>(robustness), pt, ft, rt);

    std::vector<std::uint8_t> output(input.size());
    std::size_t output_size = 0;

    // Warmup run
    decompress<PACKET_SIZE_BITS>(compressed.data(), compressed_size, output.data(), output.size(),
                                 output_size, static_cast<std::uint8_t>(robustness));

    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        decompress<PACKET_SIZE_BITS>(compressed.data(), compressed_size, output.data(),
                                     output.size(), output_size,
                                     static_cast<std::uint8_t>(robustness));
    }

    auto end = std::chrono::high_resolution_clock::now();

    double total_us = std::chrono::duration<double, std::micro>(end - start).count();
    double per_iter_us = total_us / static_cast<double>(iterations);
    double per_packet_us = per_iter_us / static_cast<double>(num_packets);
    double throughput_kbps = (static_cast<double>(input.size()) * 8.0 * 1000.0) / per_iter_us;

    std::printf("%-20s %8.2f µs/iter  %6.2f µs/pkt  %8.1f Kbps  (%zu pkts)\n", name, per_iter_us,
                per_packet_us, throughput_kbps, num_packets);
}

int main(int argc, char* argv[]) {
    int iterations = DEFAULT_ITERATIONS;

    if (argc >= 2) {
        iterations = std::atoi(argv[1]);
        if (iterations <= 0) {
            iterations = DEFAULT_ITERATIONS;
        }
    }

    std::printf("CCSDS 124.0-B-1 Benchmarks (C++ Implementation)\n");
    std::printf("=======================================\n");
    std::printf("Iterations: %d\n", iterations);
    std::printf("Packet size: %zu bits (%zu bytes)\n\n", PACKET_SIZE_BITS, PACKET_SIZE_BYTES);

    std::printf("%-20s %14s  %13s  %12s  %s\n", "Test", "Time", "Per-Packet", "Throughput",
                "Packets");
    std::printf("%-20s %14s  %13s  %12s  %s\n", "----", "----", "----------", "----------",
                "-------");

    // Try Docker path first, then local path
    const char* base_paths[] = {"/app/test-vectors/input/", "../../test-vectors/input/",
                                "../../../test-vectors/input/"};
    const char* base_path = nullptr;

    // Find working base path
    for (const auto& p : base_paths) {
        std::string test_path = std::string(p) + "simple.bin";
        std::ifstream test(test_path);
        if (test.good()) {
            base_path = p;
            break;
        }
    }

    if (base_path == nullptr) {
        std::printf("Could not find test vectors directory\n");
        return 1;
    }

    std::string simple = std::string(base_path) + "simple.bin";
    std::string hiro = std::string(base_path) + "hiro.bin";
    std::string housekeeping = std::string(base_path) + "housekeeping.bin";
    std::string venus = std::string(base_path) + "venus-express.ccsds";

    // Compression benchmarks
    std::printf("\nCompression:\n");
    bench_compress("simple", simple.c_str(), 1, 10, 20, 50, iterations);
    bench_compress("hiro", hiro.c_str(), 7, 10, 20, 50, iterations);
    bench_compress("housekeeping", housekeeping.c_str(), 2, 20, 50, 100, iterations);
    bench_compress("venus-express", venus.c_str(), 2, 20, 50, 100, iterations);

    // Decompression benchmarks
    std::printf("\nDecompression:\n");
    bench_decompress("simple", simple.c_str(), 1, 10, 20, 50, iterations);
    bench_decompress("hiro", hiro.c_str(), 7, 10, 20, 50, iterations);
    bench_decompress("housekeeping", housekeeping.c_str(), 2, 20, 50, 100, iterations);
    bench_decompress("venus-express", venus.c_str(), 2, 20, 50, 100, iterations);

    std::printf("\nNote: Desktop performance differs from embedded targets.\n");
    std::printf("Use these results for relative comparisons only.\n");

    return 0;
}

/**
 * @file cli.cpp
 * @brief CCSDS 124.0-B-1 C++ command line interface.
 *
 * @cond INTERNAL
 * ============================================================================
 *  _____                                   ____
 * |_   _|_ _ _ __   __ _  __ _ _ __ __ _  / ___| _ __   __ _  ___ ___
 *   | |/ _` | '_ \ / _` |/ _` | '__/ _` | \___ \| '_ \ / _` |/ __/ _ \
 *   | | (_| | | | | (_| | (_| | | | (_| |  ___) | |_) | (_| | (_|  __/
 *   |_|\__,_|_| |_|\__,_|\__, |_|  \__,_| |____/| .__/ \__,_|\___\___|
 *                        |___/                  |_|
 * ============================================================================
 * @endcond
 *
 * Provides a unified command-line interface for CCSDS 124.0-B-1 compression
 * and decompression.
 *
 * @authors Georges Labreche <georges@tanagraspace.com>
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#include <ccsds124/ccsds124.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

using namespace ccsds124;

static constexpr const char* BANNER = "                                                 \n"
                                      "                                                 \n"
                                      "  ____ ____ ____  ____  ____    _ ____  _  _     \n"
                                      " / ___/ ___/ ___||  _ \\/ ___|  / |___ \\| || |    \n"
                                      "| |  | |   \\___ \\| | | \\___ \\  | | __) | || |_   \n"
                                      "| |__| |___ ___) | |_| |___) | | |/ __/|__   _|  \n"
                                      " \\____\\____|____/|____/|____/  |_|_____|  |_|    \n"
                                      "                                                 \n"
                                      "          by  T A N A G R A  S P A C E           \n"
                                      "                                                 \n";

static void print_version() {
    std::printf("ccsds124 %s (C++)\n", version());
}

static void print_help(const char* prog_name) {
    std::printf("\n%s\n", BANNER);
    std::printf("CCSDS 124.0-B-1 Lossless Compression (v%s C++)\n", version());
    std::printf("=================================================\n\n");
    std::printf("References:\n");
    std::printf("  CCSDS 124.0-B-1: https://ccsds.org/Pubs/124x0b1.pdf\n");
    std::printf("  Documentation: https://tanagraspace.com/ccsds124\n\n");
    std::printf("Usage:\n");
    std::printf("  %s <input> <packet_size> <pt> <ft> <rt> <robustness>\n", prog_name);
    std::printf("  %s -d <input.pkt> <packet_size> <robustness>\n\n", prog_name);
    std::printf("Options:\n");
    std::printf("  -d             Decompress (default is compress)\n");
    std::printf("  -h, --help     Show this help message\n");
    std::printf("  -v, --version  Show version information\n\n");
    std::printf("Compress arguments:\n");
    std::printf("  input          Input file to compress\n");
    std::printf("  packet_size    Packet size in bytes (e.g., 90)\n");
    std::printf("  pt             New mask period (e.g., 10, 20)\n");
    std::printf("  ft             Send mask period (e.g., 20, 50)\n");
    std::printf("  rt             Uncompressed period (e.g., 50, 100)\n");
    std::printf("  robustness     Robustness level 0-7 (e.g., 1, 2)\n\n");
    std::printf("Decompress arguments:\n");
    std::printf("  input.pkt      Compressed input file\n");
    std::printf("  packet_size    Original packet size in bytes\n");
    std::printf("  robustness     Robustness level (must match compression)\n\n");
    std::printf("Output:\n");
    std::printf("  Compress:   <input>.pkt\n");
    std::printf("  Decompress: <input>.depkt (or <base>.depkt if input ends in .pkt)\n\n");
    std::printf("Examples:\n");
    std::printf("  %s data.bin 90 10 20 50 1        # compress\n", prog_name);
    std::printf("  %s -d data.bin.pkt 90 1          # decompress\n\n", prog_name);
}

static std::string make_decompress_filename(const std::string& input) {
    // Check if input ends with .pkt
    if (input.size() > 4 && input.substr(input.size() - 4) == ".pkt") {
        return input.substr(0, input.size() - 4) + ".depkt";
    }
    return input + ".depkt";
}

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

static bool write_file(const std::string& path, const std::uint8_t* data, std::size_t size) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return file.good();
}

static int do_compress(const char* input_path, int packet_size, int pt_period, int ft_period,
                       int rt_period, int robustness) {
    // Read input file
    auto input_data = read_file(input_path);
    if (input_data.empty()) {
        std::fprintf(stderr, "Error: Cannot read input file: %s\n", input_path);
        return 1;
    }

    std::size_t input_size = input_data.size();

    if ((input_size % static_cast<std::size_t>(packet_size)) != 0) {
        std::fprintf(stderr, "Error: Input size (%zu) not divisible by packet size (%d)\n",
                     input_size, packet_size);
        return 1;
    }

    // Create output filename
    std::string output_path = std::string(input_path) + ".pkt";

    // Allocate output buffer
    std::vector<std::uint8_t> output_data(input_size * 2);
    std::size_t output_size = 0;

    // Compress (using 720-bit packets for 90-byte packets)
    Error result;
    if (packet_size == 90) {
        result = compress<720>(
            input_data.data(), input_size, output_data.data(), output_data.size(), output_size,
            static_cast<std::uint8_t>(robustness), pt_period, ft_period, rt_period);
    } else {
        std::fprintf(stderr, "Error: Only 90-byte packets supported in CLI\n");
        return 1;
    }

    if (result != Error::Ok) {
        std::fprintf(stderr, "Error: Compression failed with code %d\n", static_cast<int>(result));
        return 1;
    }

    // Write output
    if (!write_file(output_path, output_data.data(), output_size)) {
        std::fprintf(stderr, "Error: Cannot write output file: %s\n", output_path.c_str());
        return 1;
    }

    // Print summary
    std::size_t num_packets = input_size / static_cast<std::size_t>(packet_size);
    double ratio = static_cast<double>(input_size) / static_cast<double>(output_size);
    std::printf("Input:       %s (%zu bytes, %zu packets)\n", input_path, input_size, num_packets);
    std::printf("Output:      %s (%zu bytes)\n", output_path.c_str(), output_size);
    std::printf("Ratio:       %.2fx\n", ratio);
    std::printf("Parameters:  R=%d, pt=%d, ft=%d, rt=%d\n", robustness, pt_period, ft_period,
                rt_period);

    return 0;
}

static int do_decompress(const char* input_path, int packet_size, int robustness) {
    // Read input file
    auto input_data = read_file(input_path);
    if (input_data.empty()) {
        std::fprintf(stderr, "Error: Cannot read input file: %s\n", input_path);
        return 1;
    }

    std::size_t input_size = input_data.size();

    // Create output filename
    std::string output_path = make_decompress_filename(input_path);

    // Allocate output buffer (compression ratios up to 14x observed, use 20x to be safe)
    std::vector<std::uint8_t> output_data(input_size * 20);
    std::size_t output_size = 0;

    // Decompress
    Error result;
    if (packet_size == 90) {
        result =
            decompress<720>(input_data.data(), input_size, output_data.data(), output_data.size(),
                            output_size, static_cast<std::uint8_t>(robustness));
    } else {
        std::fprintf(stderr, "Error: Only 90-byte packets supported in CLI\n");
        return 1;
    }

    if (result != Error::Ok) {
        std::fprintf(stderr, "Error: Decompression failed with code %d\n",
                     static_cast<int>(result));
        return 1;
    }

    // Write output
    if (!write_file(output_path, output_data.data(), output_size)) {
        std::fprintf(stderr, "Error: Cannot write output file: %s\n", output_path.c_str());
        return 1;
    }

    // Print summary
    std::size_t num_packets = output_size / static_cast<std::size_t>(packet_size);
    double ratio = static_cast<double>(output_size) / static_cast<double>(input_size);
    std::printf("Input:       %s (%zu bytes)\n", input_path, input_size);
    std::printf("Output:      %s (%zu bytes, %zu packets)\n", output_path.c_str(), output_size,
                num_packets);
    std::printf("Expansion:   %.2fx\n", ratio);
    std::printf("Parameters:  packet_size=%d, R=%d\n", packet_size, robustness);

    return 0;
}

int main(int argc, char** argv) {
    bool decompress_mode = false;
    int arg_offset = 1;

    // Check for help flag
    if (argc < 2 || std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        return (argc < 2) ? 1 : 0;
    }

    // Check for version flag
    if (std::strcmp(argv[1], "-v") == 0 || std::strcmp(argv[1], "--version") == 0) {
        print_version();
        return 0;
    }

    // Check for decompress flag
    if (std::strcmp(argv[1], "-d") == 0) {
        decompress_mode = true;
        arg_offset = 2;
    }

    if (decompress_mode) {
        // Decompress mode: -d <input.pkt> <packet_size> <robustness>
        if (argc != 5) {
            std::fprintf(stderr, "Error: Decompress requires 3 arguments after -d\n");
            std::fprintf(stderr, "Usage: %s -d <input.pkt> <packet_size> <robustness>\n", argv[0]);
            return 1;
        }

        const char* input_path = argv[arg_offset];
        int packet_size = std::atoi(argv[arg_offset + 1]);
        int robustness = std::atoi(argv[arg_offset + 2]);

        // Validate parameters
        if (packet_size <= 0 || packet_size > 8192) {
            std::fprintf(stderr, "Error: packet_size must be 1-8192 bytes\n");
            return 1;
        }
        if (robustness < 0 || robustness > 7) {
            std::fprintf(stderr, "Error: robustness must be 0-7\n");
            return 1;
        }

        return do_decompress(input_path, packet_size, robustness);

    } else {
        // Compress mode: <input> <packet_size> <pt> <ft> <rt> <robustness>
        if (argc != 7) {
            std::fprintf(stderr, "Error: Compress requires 6 arguments\n");
            std::fprintf(stderr, "Usage: %s <input> <packet_size> <pt> <ft> <rt> <robustness>\n",
                         argv[0]);
            return 1;
        }

        const char* input_path = argv[1];
        int packet_size = std::atoi(argv[2]);
        int pt_period = std::atoi(argv[3]);
        int ft_period = std::atoi(argv[4]);
        int rt_period = std::atoi(argv[5]);
        int robustness = std::atoi(argv[6]);

        // Validate parameters
        if (packet_size <= 0 || packet_size > 8192) {
            std::fprintf(stderr, "Error: packet_size must be 1-8192 bytes\n");
            return 1;
        }
        if (robustness < 0 || robustness > 7) {
            std::fprintf(stderr, "Error: robustness must be 0-7\n");
            return 1;
        }
        if (pt_period <= 0 || ft_period <= 0 || rt_period <= 0) {
            std::fprintf(stderr, "Error: periods must be positive\n");
            return 1;
        }

        return do_compress(input_path, packet_size, pt_period, ft_period, rt_period, robustness);
    }
}

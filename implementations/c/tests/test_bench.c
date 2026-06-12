/**
 * @file test_bench.c
 * @brief Performance benchmarks for CCSDS 124.0-B-1 compression.
 *
 * Measures compression throughput for regression testing during development.
 * Note: Desktop performance differs from AVR32 target - use for relative
 * comparisons only.
 *
 * Usage:
 *   make bench              # Run with default 100 iterations
 *   ./build/test_bench 1000 # Run with custom iteration count
 */

/* Enable POSIX features for clock_gettime */
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ccsds124.h"

#define DEFAULT_ITERATIONS 100
#define PACKET_SIZE_BITS 720

static double time_diff_us(struct timespec start, struct timespec end) {
    double sec_diff = (double)(end.tv_sec - start.tv_sec);
    double nsec_diff = (double)(end.tv_nsec - start.tv_nsec);
    return (sec_diff * 1e6) + (nsec_diff / 1e3);
}

static int load_file(const char *path, uint8_t **data, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }

    (void)fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    (void)fseek(f, 0, SEEK_SET);

    *data = malloc(*size);
    if (*data == NULL) {
        (void)fclose(f);
        return -1;
    }

    size_t read = fread(*data, 1U, *size, f);
    (void)fclose(f);

    if (read != *size) {
        free(*data);
        return -1;
    }

    return 0;
}

static void bench_compress(const char *name, const char *path, int robustness,
                           int pt, int ft, int rt, int iterations) {
    uint8_t *input = NULL;
    size_t input_size = 0;

    if (load_file(path, &input, &input_size) != 0) {
        printf("%-20s SKIP (file not found)\n", name);
        return;
    }

    size_t num_packets = input_size / (PACKET_SIZE_BITS / 8U);
    uint8_t *output = malloc(input_size * 2U);
    size_t output_size = 0;
    ccsds124_compressor_t comp;

    /* Warmup run */
    (void)ccsds124_compressor_init(&comp, PACKET_SIZE_BITS, NULL,
                                  robustness, pt, ft, rt);
    (void)ccsds124_compress(&comp, input, input_size, output,
                          input_size * 2U, &output_size);

    /* Benchmark */
    struct timespec start, end;
    (void)clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < iterations; i++) {
        (void)ccsds124_compressor_init(&comp, PACKET_SIZE_BITS, NULL,
                                      robustness, pt, ft, rt);
        (void)ccsds124_compress(&comp, input, input_size, output,
                              input_size * 2U, &output_size);
    }

    (void)clock_gettime(CLOCK_MONOTONIC, &end);

    double total_us = time_diff_us(start, end);
    double per_iter_us = total_us / (double)iterations;
    double per_packet_us = per_iter_us / (double)num_packets;
    double throughput_kbps = ((double)input_size * 8.0 * 1000.0) / per_iter_us;

    printf("%-20s %8.2f µs/iter  %6.2f µs/pkt  %8.1f Kbps  (%zu pkts)\n",
           name, per_iter_us, per_packet_us, throughput_kbps, num_packets);

    free(input);
    free(output);
}

static void bench_decompress(const char *name, const char *path, int robustness,
                             int pt, int ft, int rt, int iterations) {
    uint8_t *input = NULL;
    size_t input_size = 0;

    if (load_file(path, &input, &input_size) != 0) {
        printf("%-20s SKIP (file not found)\n", name);
        return;
    }

    size_t num_packets = input_size / (PACKET_SIZE_BITS / 8U);

    /* First compress the data */
    uint8_t *compressed = malloc(input_size * 2U);
    size_t compressed_size = 0;
    ccsds124_compressor_t comp;
    (void)ccsds124_compressor_init(&comp, PACKET_SIZE_BITS, NULL,
                                  robustness, pt, ft, rt);
    (void)ccsds124_compress(&comp, input, input_size, compressed,
                          input_size * 2U, &compressed_size);

    uint8_t *output = malloc(input_size);
    size_t output_size = 0;
    ccsds124_decompressor_t decomp;

    /* Warmup run */
    (void)ccsds124_decompressor_init(&decomp, PACKET_SIZE_BITS, NULL, robustness);
    (void)ccsds124_decompress(&decomp, compressed, compressed_size, output,
                            input_size, &output_size);

    /* Benchmark */
    struct timespec start, end;
    (void)clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < iterations; i++) {
        (void)ccsds124_decompressor_init(&decomp, PACKET_SIZE_BITS, NULL,
                                        robustness);
        (void)ccsds124_decompress(&decomp, compressed, compressed_size, output,
                                input_size, &output_size);
    }

    (void)clock_gettime(CLOCK_MONOTONIC, &end);

    double total_us = time_diff_us(start, end);
    double per_iter_us = total_us / (double)iterations;
    double per_packet_us = per_iter_us / (double)num_packets;
    double throughput_kbps = ((double)input_size * 8.0 * 1000.0) / per_iter_us;

    printf("%-20s %8.2f µs/iter  %6.2f µs/pkt  %8.1f Kbps  (%zu pkts)\n",
           name, per_iter_us, per_packet_us, throughput_kbps, num_packets);

    free(input);
    free(compressed);
    free(output);
}

int main(int argc, char *argv[]) {
    int iterations = DEFAULT_ITERATIONS;

    if (argc >= 2) {
        iterations = atoi(argv[1]);
        if (iterations <= 0) {
            iterations = DEFAULT_ITERATIONS;
        }
    }

    printf("CCSDS 124.0-B-1 Benchmarks (C Implementation)\n");
    printf("=====================================\n");
    printf("Iterations: %d\n", iterations);
    printf("Packet size: %d bits (%d bytes)\n\n",
           PACKET_SIZE_BITS, PACKET_SIZE_BITS / 8);

    printf("%-20s %14s  %13s  %12s  %s\n",
           "Test", "Time", "Per-Packet", "Throughput", "Packets");
    printf("%-20s %14s  %13s  %12s  %s\n",
           "----", "----", "----------", "----------", "-------");

    /* Compression benchmarks (matching Go benchmark_test.go) */
    printf("\nCompression:\n");
    bench_compress("simple",
                   "../../test-vectors/input/simple.bin",
                   1, 10, 20, 50, iterations);
    bench_compress("hiro",
                   "../../test-vectors/input/hiro.bin",
                   7, 10, 20, 50, iterations);
    bench_compress("housekeeping",
                   "../../test-vectors/input/housekeeping.bin",
                   2, 20, 50, 100, iterations);
    bench_compress("venus-express",
                   "../../test-vectors/input/venus-express.ccsds",
                   2, 20, 50, 100, iterations);

    /* Decompression benchmarks */
    printf("\nDecompression:\n");
    bench_decompress("simple",
                     "../../test-vectors/input/simple.bin",
                     1, 10, 20, 50, iterations);
    bench_decompress("hiro",
                     "../../test-vectors/input/hiro.bin",
                     7, 10, 20, 50, iterations);
    bench_decompress("housekeeping",
                     "../../test-vectors/input/housekeeping.bin",
                     2, 20, 50, 100, iterations);
    bench_decompress("venus-express",
                     "../../test-vectors/input/venus-express.ccsds",
                     2, 20, 50, 100, iterations);

    printf("\nNote: Desktop performance differs from AVR32 target.\n");
    printf("Use these results for relative comparisons only.\n");

    return 0;
}

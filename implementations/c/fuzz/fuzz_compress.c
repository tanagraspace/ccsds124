/**
 * @file fuzz_compress.c
 * @brief libFuzzer harness for CCSDS 124.0-B-1 compression.
 *
 * Tests the compressor with arbitrary input data to find crashes,
 * hangs, and memory safety issues.
 *
 * Build with libFuzzer:
 *   clang -g -O1 -fno-omit-frame-pointer -fsanitize=fuzzer,address \
 *         -I../include fuzz_compress.c ../src/*.c -o fuzz_compress
 *
 * Build with AFL++:
 *   afl-clang-fast -g -O1 -I../include fuzz_compress.c ../src/*.c \
 *         -o fuzz_compress_afl -DAFL_HARNESS
 *
 * Run:
 *   ./fuzz_compress corpus/ -max_len=8192
 */

#include "ccsds124.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifdef AFL_HARNESS
#include <stdio.h>
#include <unistd.h>

/* AFL++ harness: read from stdin */
int main(void) {
    uint8_t data[65536];
    ssize_t len = read(STDIN_FILENO, data, sizeof(data));
    if (len <= 0) return 0;

    /* Extract parameters from first 4 bytes if available */
    if (len < 5) return 0;

    uint8_t r = data[0] & 0x07;  /* Robustness 0-7 */
    uint16_t F_raw = (uint16_t)(data[1] << 8) | data[2];
    size_t F = (F_raw % 512) + 8;  /* Packet size 8-519 bits, byte aligned */
    F = (F / 8) * 8;  /* Ensure byte alignment */
    if (F == 0) F = 8;

    uint8_t pt = data[3] % 16;
    uint8_t ft = data[3] / 16;

    size_t packet_bytes = F / 8;
    size_t input_bytes = (size_t)len - 4;
    input_bytes = (input_bytes / packet_bytes) * packet_bytes;  /* Align to packets */
    if (input_bytes == 0) return 0;

    ccsds124_compressor_t comp;
    if (ccsds124_compressor_init(&comp, F, NULL, r, pt, ft, 0) != CCSDS124_OK) {
        return 0;
    }

    size_t output_size = input_bytes * 3 + 1024;
    uint8_t *output = malloc(output_size);
    if (!output) return 0;

    size_t actual_output = 0;
    (void)ccsds124_compress(&comp, data + 4, input_bytes,
                           output, output_size, &actual_output);

    free(output);
    return 0;
}

#else
/* libFuzzer harness */

/**
 * @brief libFuzzer entry point.
 *
 * Fuzzes the compressor with arbitrary byte sequences.
 * Uses first bytes to derive parameters, rest as input data.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 5) {
        return 0;  /* Need at least header + some data */
    }

    /* Extract parameters from first bytes */
    uint8_t r = data[0] & 0x07;  /* Robustness 0-7 */
    uint16_t F_raw = (uint16_t)(data[1] << 8) | data[2];
    size_t F = (F_raw % 512) + 8;  /* Packet size 8-519 bits */
    F = (F / 8) * 8;  /* Ensure byte alignment for simplicity */
    if (F == 0) {
        F = 8;
    }

    uint8_t pt = data[3] % 16;
    uint8_t ft = data[3] / 16;

    /* Calculate input size as multiple of packet size */
    size_t packet_bytes = F / 8;
    size_t input_bytes = size - 4;
    input_bytes = (input_bytes / packet_bytes) * packet_bytes;
    if (input_bytes == 0) {
        return 0;
    }

    /* Initialize compressor */
    ccsds124_compressor_t comp;
    if (ccsds124_compressor_init(&comp, F, NULL, r, (int)pt, (int)ft, 0) != CCSDS124_OK) {
        return 0;
    }

    /* Allocate output buffer */
    size_t output_size = input_bytes * 3 + 1024;  /* Generous buffer */
    uint8_t *output = (uint8_t *)malloc(output_size);
    if (!output) {
        return 0;
    }

    /* Attempt compression - we don't care about the result,
     * just that it doesn't crash or hang */
    size_t actual_output = 0;
    (void)ccsds124_compress(&comp, data + 4, input_bytes,
                           output, output_size, &actual_output);

    free(output);
    return 0;
}

#endif /* AFL_HARNESS */

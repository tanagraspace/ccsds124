/**
 * @file fuzz_decompress.c
 * @brief libFuzzer harness for CCSDS 124.0-B-1 decompression.
 *
 * Tests the decompressor with arbitrary input data to find crashes,
 * hangs, and memory safety issues.
 *
 * Build with libFuzzer:
 *   clang -g -O1 -fno-omit-frame-pointer -fsanitize=fuzzer,address \
 *         -I../include fuzz_decompress.c ../src/*.c -o fuzz_decompress
 *
 * Build with AFL++:
 *   afl-clang-fast -g -O1 -I../include fuzz_decompress.c ../src/*.c \
 *         -o fuzz_decompress_afl -DAFL_HARNESS
 *
 * Run:
 *   ./fuzz_decompress corpus/ -max_len=4096
 */

#include "ccsds124.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef AFL_HARNESS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* AFL++ harness: read from stdin */
int main(void) {
    uint8_t data[65536];
    ssize_t len = read(STDIN_FILENO, data, sizeof(data));
    if (len <= 0) return 0;

    /* Extract parameters from first 3 bytes if available */
    if (len < 3) return 0;

    uint8_t r = data[0] & 0x07;  /* Robustness 0-7 */
    uint16_t F_raw = (uint16_t)(data[1] << 8) | data[2];
    size_t F = (F_raw % 1024) + 8;  /* Packet size 8-1031 bits */

    ccsds124_decompressor_t decomp;
    if (ccsds124_decompressor_init(&decomp, F, NULL, r) != CCSDS124_OK) {
        return 0;
    }

    size_t packet_bytes = (F + 7) / 8;
    size_t max_packets = 1024;
    size_t output_size = packet_bytes * max_packets;
    uint8_t *output = malloc(output_size);
    if (!output) return 0;

    size_t actual_output = 0;
    (void)ccsds124_decompress(&decomp, data + 3, (size_t)len - 3,
                             output, output_size, &actual_output);

    free(output);
    return 0;
}

#else
/* libFuzzer harness */

/**
 * @brief libFuzzer entry point.
 *
 * Fuzzes the decompressor with arbitrary byte sequences.
 * Uses first bytes to derive parameters, rest as compressed data.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) {
        return 0;  /* Need at least header + some data */
    }

    /* Extract parameters from first bytes */
    uint8_t r = data[0] & 0x07;  /* Robustness 0-7 */
    uint16_t F_raw = (uint16_t)(data[1] << 8) | data[2];
    size_t F = (F_raw % 1024) + 8;  /* Packet size 8-1031 bits */

    /* Initialize decompressor */
    ccsds124_decompressor_t decomp;
    if (ccsds124_decompressor_init(&decomp, F, NULL, r) != CCSDS124_OK) {
        return 0;
    }

    /* Allocate output buffer */
    size_t packet_bytes = (F + 7) / 8;
    size_t max_packets = 1024;
    size_t output_size = packet_bytes * max_packets;
    uint8_t *output = (uint8_t *)malloc(output_size);
    if (!output) {
        return 0;
    }

    /* Attempt decompression - we don't care about the result,
     * just that it doesn't crash or hang */
    size_t actual_output = 0;
    (void)ccsds124_decompress(&decomp, data + 3, size - 3,
                             output, output_size, &actual_output);

    free(output);
    return 0;
}

#endif /* AFL_HARNESS */

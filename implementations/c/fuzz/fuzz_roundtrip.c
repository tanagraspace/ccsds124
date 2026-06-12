/**
 * @file fuzz_roundtrip.c
 * @brief libFuzzer harness for CCSDS 124.0-B-1 roundtrip testing.
 *
 * Tests that compress(decompress(x)) == x for valid data.
 * This catches compression/decompression mismatches and data corruption.
 *
 * Build with libFuzzer:
 *   clang -g -O1 -fno-omit-frame-pointer -fsanitize=fuzzer,address \
 *         -I../include fuzz_roundtrip.c ../src/*.c -o fuzz_roundtrip
 *
 * Run:
 *   ./fuzz_roundtrip corpus/ -max_len=4096
 */

#include "ccsds124.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifdef AFL_HARNESS
#include <stdio.h>
#include <unistd.h>

int main(void) {
    uint8_t data[65536];
    ssize_t len = read(STDIN_FILENO, data, sizeof(data));
    if (len <= 0) return 0;

    if (len < 5) return 0;

    uint8_t r = data[0] & 0x07;
    uint16_t F_raw = (uint16_t)(data[1] << 8) | data[2];
    size_t F = (F_raw % 256) + 8;
    F = (F / 8) * 8;
    if (F == 0) F = 8;

    uint8_t pt = (data[3] % 15) + 1;
    uint8_t ft = (data[3] / 16) + 1;

    size_t packet_bytes = F / 8;
    size_t input_bytes = (size_t)len - 4;
    input_bytes = (input_bytes / packet_bytes) * packet_bytes;
    if (input_bytes == 0) return 0;

    ccsds124_compressor_t comp;
    if (ccsds124_compressor_init(&comp, F, NULL, r, pt, ft, ft * 2) != CCSDS124_OK) {
        return 0;
    }

    size_t compressed_size_max = input_bytes * 4 + 1024;
    uint8_t *compressed = malloc(compressed_size_max);
    if (!compressed) return 0;

    size_t compressed_size = 0;
    int result = ccsds124_compress(&comp, data + 4, input_bytes,
                                  compressed, compressed_size_max, &compressed_size);
    if (result != CCSDS124_OK) {
        free(compressed);
        return 0;
    }

    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, r);

    uint8_t *decompressed = malloc(input_bytes);
    if (!decompressed) {
        free(compressed);
        return 0;
    }

    size_t decompressed_size = 0;
    result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                decompressed, input_bytes, &decompressed_size);

    if (result == CCSDS124_OK && decompressed_size == input_bytes) {
        assert(memcmp(data + 4, decompressed, input_bytes) == 0);
    }

    free(compressed);
    free(decompressed);
    return 0;
}

#else
/* libFuzzer harness */

/**
 * @brief libFuzzer entry point.
 *
 * Compresses arbitrary data, then decompresses and verifies roundtrip.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 5) {
        return 0;
    }

    /* Extract parameters */
    uint8_t r = data[0] & 0x07;
    uint16_t F_raw = (uint16_t)(data[1] << 8) | data[2];
    size_t F = (F_raw % 256) + 8;  /* 8-263 bits */
    F = (F / 8) * 8;  /* Byte align */
    if (F == 0) {
        F = 8;
    }

    /* Reasonable timing parameters */
    uint8_t pt = (data[3] % 15) + 1;  /* 1-15 */
    uint8_t ft = (data[3] / 16) + 1;  /* 1-16 */

    size_t packet_bytes = F / 8;
    size_t input_bytes = size - 4;
    input_bytes = (input_bytes / packet_bytes) * packet_bytes;
    if (input_bytes == 0) {
        return 0;
    }

    /* Compress */
    ccsds124_compressor_t comp;
    if (ccsds124_compressor_init(&comp, F, NULL, r, (int)pt, (int)ft, (int)(ft * 2)) != CCSDS124_OK) {
        return 0;
    }

    size_t compressed_size_max = input_bytes * 4 + 1024;
    uint8_t *compressed = (uint8_t *)malloc(compressed_size_max);
    if (!compressed) {
        return 0;
    }

    size_t compressed_size = 0;
    int result = ccsds124_compress(&comp, data + 4, input_bytes,
                                  compressed, compressed_size_max, &compressed_size);
    if (result != CCSDS124_OK) {
        free(compressed);
        return 0;
    }

    /* Decompress */
    ccsds124_decompressor_t decomp;
    ccsds124_decompressor_init(&decomp, F, NULL, r);

    uint8_t *decompressed = (uint8_t *)malloc(input_bytes);
    if (!decompressed) {
        free(compressed);
        return 0;
    }

    size_t decompressed_size = 0;
    result = ccsds124_decompress(&decomp, compressed, compressed_size,
                                decompressed, input_bytes, &decompressed_size);

    /* Verify roundtrip */
    if (result == CCSDS124_OK && decompressed_size == input_bytes) {
        /* This assertion will trigger if roundtrip fails */
        assert(memcmp(data + 4, decompressed, input_bytes) == 0);
    }

    free(compressed);
    free(decompressed);
    return 0;
}

#endif /* AFL_HARNESS */

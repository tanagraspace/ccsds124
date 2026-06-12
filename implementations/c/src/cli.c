/**
 * @file cli.c
 * @brief CCSDS 124.0-B-1 unified command line interface.
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
 * and decompression, similar to gzip.
 *
 * @par Usage
 * @code
 * ccsds124 [options] <input> <packet_size> [compress_params]
 * @endcode
 *
 * @authors Georges Labreche <georges@tanagraspace.com> - https://georges.fyi
 * @authors Claude Code (Anthropic) <noreply@anthropic.com>
 *
 * @see https://ccsds.org/Pubs/124x0b1.pdf CCSDS 124.0-B-1 Standard
 */

#include "ccsds124.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief ASCII art banner for help output. */
static const char *BANNER =
"                                                 \n"
"                                                 \n"
"  ____ ____ ____  ____  ____    _ ____  _  _     \n"
" / ___/ ___/ ___||  _ \\/ ___|  / |___ \\| || |    \n"
"| |  | |   \\___ \\| | | \\___ \\  | | __) | || |_   \n"
"| |__| |___ ___) | |_| |___) | | |/ __/|__   _|  \n"
" \\____\\____|____/|____/|____/  |_|_____|  |_|    \n"
"                                                 \n"
"          by  T A N A G R A  S P A C E           \n"
"                                                 \n";

/**
 * @brief Print help message with usage information.
 *
 * @param[in] prog_name Program name for usage example.
 */
static void print_version(void) {
    printf("ccsds124 %d.%d.%d\n",
           CCSDS124_VERSION_MAJOR,
           CCSDS124_VERSION_MINOR,
           CCSDS124_VERSION_PATCH);
}

static void print_help(const char *prog_name) {
    printf("\n%s\n", BANNER);
    printf("CCSDS 124.0-B-1 Lossless Compression (v%d.%d.%d)\n",
           CCSDS124_VERSION_MAJOR, CCSDS124_VERSION_MINOR, CCSDS124_VERSION_PATCH);
    printf("=================================================\n\n");
    printf("References:\n");
    printf("  CCSDS 124.0-B-1: https://ccsds.org/Pubs/124x0b1.pdf\n");
    printf("  Documentation: https://tanagraspace.com/ccsds124\n\n");
    printf("Citation:\n");
    printf("  D. Evans, G. Labreche, D. Marszk, S. Bammens, M. Hernandez-Cabronero,\n");
    printf("  V. Zelenevskiy, V. Shiradhonkar, M. Starcik, and M. Henkel. 2022.\n");
    printf("  \"Implementing the New CCSDS Housekeeping Data Compression Standard\n");
    printf("  124.0-B-1 (based on POCKET+) on OPS-SAT-1,\" Proceedings of the\n");
    printf("  Small Satellite Conference, Communications, SSC22-XII-03.\n");
    printf("  https://digitalcommons.usu.edu/smallsat/2022/all2022/133/\n\n");
    printf("Usage:\n");
    printf("  %s <input> <packet_size> <pt> <ft> <rt> <robustness>\n", prog_name);
    printf("  %s -d <input.pkt> <packet_size> <robustness>\n\n", prog_name);
    printf("Options:\n");
    printf("  -d             Decompress (default is compress)\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -v, --version  Show version information\n\n");
    printf("Compress arguments:\n");
    printf("  input          Input file to compress\n");
    printf("  packet_size    Packet size in bytes (e.g., 90)\n");
    printf("  pt             New mask period (e.g., 10, 20)\n");
    printf("  ft             Send mask period (e.g., 20, 50)\n");
    printf("  rt             Uncompressed period (e.g., 50, 100)\n");
    printf("  robustness     Robustness level 0-7 (e.g., 1, 2)\n\n");
    printf("Decompress arguments:\n");
    printf("  input.pkt      Compressed input file\n");
    printf("  packet_size    Original packet size in bytes\n");
    printf("  robustness     Robustness level (must match compression)\n\n");
    printf("Output:\n");
    printf("  Compress:   <input>.pkt\n");
    printf("  Decompress: <input>.depkt (or <base>.depkt if input ends in .pkt)\n\n");
    printf("Examples:\n");
    printf("  %s data.bin 90 10 20 50 1        # compress\n", prog_name);
    printf("  %s -d data.bin.pkt 90 1          # decompress\n\n", prog_name);
}

/**
 * @brief Create output filename for decompression.
 *
 * Removes .pkt extension if present, then appends .depkt.
 *
 * @param[out] output     Output buffer.
 * @param[in]  output_len Buffer size.
 * @param[in]  input      Input filename.
 */
static void make_decompress_filename(char *output, size_t output_len, const char *input) {
    size_t len = strlen(input);

    /* Check if input ends with .pkt */
    if ((len > 4) && (strcmp(&input[len - 4], ".pkt") == 0)) {
        /* Remove .pkt and add .depkt */
        size_t base_len = len - 4;
        if ((base_len + 7) < output_len) {
            (void)strncpy(output, input, base_len);
            output[base_len] = '\0';
            (void)strcat(output, ".depkt");
        } else {
            (void)snprintf(output, output_len, "%s.depkt", input);
        }
    } else {
        (void)snprintf(output, output_len, "%s.depkt", input);
    }
}

/**
 * @brief Compress a file.
 *
 * @param[in] input_path   Input file path.
 * @param[in] packet_size  Packet size in bytes.
 * @param[in] pt_period    New mask period.
 * @param[in] ft_period    Send mask period.
 * @param[in] rt_period    Uncompressed period.
 * @param[in] robustness   Robustness level (0-7).
 *
 * @return 0 on success, 1 on error.
 */
static int do_compress(const char *input_path, int packet_size,
                       int pt_period, int ft_period, int rt_period,
                       int robustness) {
    /* Read input file */
    FILE *fin = fopen(input_path, "rb");
    if (fin == NULL) {
        fprintf(stderr, "Error: Cannot open input file: %s\n", input_path);
        return 1;
    }

    (void)fseek(fin, 0, SEEK_END);
    long file_len = ftell(fin);
    (void)fseek(fin, 0, SEEK_SET);

    if (file_len <= 0) {
        fprintf(stderr, "Error: Input file is empty or invalid\n");
        (void)fclose(fin);
        return 1;
    }

    size_t input_size = (size_t)file_len;

    if ((input_size % (size_t)packet_size) != 0U) {
        fprintf(stderr, "Error: Input size (%zu) not divisible by packet size (%d)\n",
                input_size, packet_size);
        (void)fclose(fin);
        return 1;
    }

    uint8_t *input_data = malloc(input_size);
    if (input_data == NULL) {
        fprintf(stderr, "Error: Cannot allocate memory\n");
        (void)fclose(fin);
        return 1;
    }

    if (fread(input_data, 1, input_size, fin) != input_size) {
        fprintf(stderr, "Error: Failed to read input file\n");
        (void)fclose(fin);
        free(input_data);
        return 1;
    }
    (void)fclose(fin);

    /* Create output filename */
    char output_path[1024];
    (void)snprintf(output_path, sizeof(output_path), "%s.pkt", input_path);

    /* Initialize compressor */
    size_t packet_length = (size_t)packet_size * 8U;
    ccsds124_compressor_t comp;
    (void)ccsds124_compressor_init(&comp, packet_length, NULL, (uint8_t)robustness,
                                  pt_period, ft_period, rt_period);

    /* Allocate output buffer */
    size_t max_output = input_size * 2U;
    uint8_t *output_data = malloc(max_output);
    if (output_data == NULL) {
        fprintf(stderr, "Error: Cannot allocate output buffer\n");
        free(input_data);
        return 1;
    }

    /* Compress */
    size_t output_size = 0U;
    int result = ccsds124_compress(&comp, input_data, input_size,
                                  output_data, max_output, &output_size);

    if (result != CCSDS124_OK) {
        fprintf(stderr, "Error: Compression failed with code %d\n", result);
        free(input_data);
        free(output_data);
        return 1;
    }

    /* Write output */
    FILE *fout = fopen(output_path, "wb");
    if (fout == NULL) {
        fprintf(stderr, "Error: Cannot create output file: %s\n", output_path);
        free(input_data);
        free(output_data);
        return 1;
    }

    if (fwrite(output_data, 1, output_size, fout) != output_size) {
        fprintf(stderr, "Error: Failed to write output file\n");
        (void)fclose(fout);
        free(input_data);
        free(output_data);
        return 1;
    }
    (void)fclose(fout);

    /* Print summary */
    size_t num_packets = input_size / (size_t)packet_size;
    double ratio = (double)input_size / (double)output_size;
    printf("Input:       %s (%zu bytes, %zu packets)\n", input_path, input_size, num_packets);
    printf("Output:      %s (%zu bytes)\n", output_path, output_size);
    printf("Ratio:       %.2fx\n", ratio);
    printf("Parameters:  R=%d, pt=%d, ft=%d, rt=%d\n",
           robustness, pt_period, ft_period, rt_period);

    free(input_data);
    free(output_data);
    return 0;
}

/**
 * @brief Decompress a file.
 *
 * @param[in] input_path   Compressed input file path.
 * @param[in] packet_size  Original packet size in bytes.
 * @param[in] robustness   Robustness level (0-7).
 *
 * @return 0 on success, 1 on error.
 */
static int do_decompress(const char *input_path, int packet_size, int robustness) {
    /* Read input file */
    FILE *fin = fopen(input_path, "rb");
    if (fin == NULL) {
        fprintf(stderr, "Error: Cannot open input file: %s\n", input_path);
        return 1;
    }

    (void)fseek(fin, 0, SEEK_END);
    long file_len = ftell(fin);
    (void)fseek(fin, 0, SEEK_SET);

    if (file_len <= 0) {
        fprintf(stderr, "Error: Input file is empty or invalid\n");
        (void)fclose(fin);
        return 1;
    }

    size_t input_size = (size_t)file_len;
    uint8_t *input_data = malloc(input_size);
    if (input_data == NULL) {
        fprintf(stderr, "Error: Cannot allocate memory for input\n");
        (void)fclose(fin);
        return 1;
    }

    if (fread(input_data, 1, input_size, fin) != input_size) {
        fprintf(stderr, "Error: Failed to read input file\n");
        (void)fclose(fin);
        free(input_data);
        return 1;
    }
    (void)fclose(fin);

    /* Create output filename */
    char output_path[1024];
    make_decompress_filename(output_path, sizeof(output_path), input_path);

    /* Initialize decompressor */
    size_t packet_length = (size_t)packet_size * 8U;
    ccsds124_decompressor_t decomp;
    int result = ccsds124_decompressor_init(&decomp, packet_length, NULL, (uint8_t)robustness);

    if (result != CCSDS124_OK) {
        fprintf(stderr, "Error: Decompressor init failed with code %d\n", result);
        free(input_data);
        return 1;
    }

    /* Allocate output buffer (compression ratios up to 14x observed, use 20x to be safe) */
    size_t max_output = input_size * 20U;
    uint8_t *output_data = malloc(max_output);
    if (output_data == NULL) {
        fprintf(stderr, "Error: Cannot allocate output buffer\n");
        free(input_data);
        return 1;
    }

    /* Decompress */
    size_t output_size = 0U;
    result = ccsds124_decompress(&decomp, input_data, input_size,
                               output_data, max_output, &output_size);

    if (result != CCSDS124_OK) {
        fprintf(stderr, "Error: Decompression failed with code %d\n", result);
        free(input_data);
        free(output_data);
        return 1;
    }

    /* Write output file */
    FILE *fout = fopen(output_path, "wb");
    if (fout == NULL) {
        fprintf(stderr, "Error: Cannot create output file: %s\n", output_path);
        free(input_data);
        free(output_data);
        return 1;
    }

    if (fwrite(output_data, 1, output_size, fout) != output_size) {
        fprintf(stderr, "Error: Failed to write output file\n");
        (void)fclose(fout);
        free(input_data);
        free(output_data);
        return 1;
    }
    (void)fclose(fout);

    /* Print summary */
    size_t num_packets = output_size / (size_t)packet_size;
    double ratio = (double)output_size / (double)input_size;
    printf("Input:       %s (%zu bytes)\n", input_path, input_size);
    printf("Output:      %s (%zu bytes, %zu packets)\n", output_path, output_size, num_packets);
    printf("Expansion:   %.2fx\n", ratio);
    printf("Parameters:  packet_size=%d, R=%d\n", packet_size, robustness);

    free(input_data);
    free(output_data);
    return 0;
}

/**
 * @brief CLI entry point.
 *
 * @param[in] argc Argument count.
 * @param[in] argv Argument vector.
 *
 * @return 0 on success, 1 on error.
 */
int main(int argc, char **argv) {
    int decompress_mode = 0;
    int arg_offset = 1;

    /* Check for help flag */
    if ((argc < 2) ||
        (strcmp(argv[1], "-h") == 0) ||
        (strcmp(argv[1], "--help") == 0)) {
        print_help(argv[0]);
        return (argc < 2) ? 1 : 0;
    }

    /* Check for version flag */
    if ((strcmp(argv[1], "-v") == 0) ||
        (strcmp(argv[1], "--version") == 0)) {
        print_version();
        return 0;
    }

    /* Check for decompress flag */
    if (strcmp(argv[1], "-d") == 0) {
        decompress_mode = 1;
        arg_offset = 2;
    }

    if (decompress_mode != 0) {
        /* Decompress mode: -d <input.pkt> <packet_size> <robustness> */
        if (argc != 5) {
            fprintf(stderr, "Error: Decompress requires 3 arguments after -d\n");
            fprintf(stderr, "Usage: %s -d <input.pkt> <packet_size> <robustness>\n", argv[0]);
            return 1;
        }

        const char *input_path = argv[arg_offset];
        int packet_size = atoi(argv[arg_offset + 1]);
        int robustness = atoi(argv[arg_offset + 2]);

        /* Validate parameters */
        if ((packet_size <= 0) || (packet_size > 8192)) {
            fprintf(stderr, "Error: packet_size must be 1-8192 bytes\n");
            return 1;
        }
        if ((robustness < 0) || (robustness > 7)) {
            fprintf(stderr, "Error: robustness must be 0-7\n");
            return 1;
        }

        return do_decompress(input_path, packet_size, robustness);

    } else {
        /* Compress mode: <input> <packet_size> <pt> <ft> <rt> <robustness> */
        if (argc != 7) {
            fprintf(stderr, "Error: Compress requires 6 arguments\n");
            fprintf(stderr, "Usage: %s <input> <packet_size> <pt> <ft> <rt> <robustness>\n", argv[0]);
            return 1;
        }

        const char *input_path = argv[1];
        int packet_size = atoi(argv[2]);
        int pt_period = atoi(argv[3]);
        int ft_period = atoi(argv[4]);
        int rt_period = atoi(argv[5]);
        int robustness = atoi(argv[6]);

        /* Validate parameters */
        if ((packet_size <= 0) || (packet_size > 8192)) {
            fprintf(stderr, "Error: packet_size must be 1-8192 bytes\n");
            return 1;
        }
        if ((robustness < 0) || (robustness > 7)) {
            fprintf(stderr, "Error: robustness must be 0-7\n");
            return 1;
        }
        if ((pt_period <= 0) || (ft_period <= 0) || (rt_period <= 0)) {
            fprintf(stderr, "Error: periods must be positive\n");
            return 1;
        }

        return do_compress(input_path, packet_size, pt_period, ft_period, rt_period, robustness);
    }
}

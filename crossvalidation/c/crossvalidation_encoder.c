/**
 * @file crossvalidation_encoder.c
 * @brief Cross-validation encoder harness for CCSDS 124.0-B-1.
 *
 * Reads a .raw+config input file, compresses each packet using the
 * ccsds124_compress_packet() API with per-packet flags, and writes
 * the concatenated byte-aligned compressed output to a .124 file.
 *
 * Usage: crossvalidation_encoder <input.raw+config> <output.124>
 */

#include "ccsds124.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Read a 32-bit big-endian unsigned integer from a byte buffer. */
static uint32_t read_be32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           ((uint32_t)buf[3]);
}

/** Check that padding bits (beyond large_f) in a byte buffer are zero. */
static int check_padding_zero(const uint8_t *buf, uint32_t large_f) {
    uint32_t total_bits = ((large_f + 7U) / 8U) * 8U;
    uint32_t padding_bits = total_bits - large_f;

    if (padding_bits == 0U) {
        return 1; /* No padding, always valid */
    }

    /* Check the last byte's lower padding_bits are zero */
    uint32_t num_bytes = (large_f + 7U) / 8U;
    uint8_t last_byte = buf[num_bytes - 1U];
    uint8_t mask = (uint8_t)((1U << padding_bits) - 1U);

    return (last_byte & mask) == 0U;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.raw+config> <output.124>\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];

    /* Read entire input file */
    FILE *fin = fopen(input_path, "rb");
    if (fin == NULL) {
        fprintf(stderr, "Error: cannot open %s\n", input_path);
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fin);
        /* Empty input -> empty output */
        FILE *fout = fopen(output_path, "wb");
        if (fout != NULL) { fclose(fout); }
        return 0;
    }

    uint8_t *file_data = (uint8_t *)malloc((size_t)file_size);
    if (file_data == NULL) {
        fclose(fin);
        fprintf(stderr, "Error: malloc failed\n");
        return 1;
    }

    size_t bytes_read = fread(file_data, 1, (size_t)file_size, fin);
    fclose(fin);

    if (bytes_read != (size_t)file_size) {
        free(file_data);
        fprintf(stderr, "Error: short read\n");
        return 1;
    }

    /* Allocate output buffer (generous: 6x input should be enough) */
    size_t output_capacity = (size_t)file_size * 6U + 1024U;
    uint8_t *output_data = (uint8_t *)malloc(output_capacity);
    if (output_data == NULL) {
        free(file_data);
        fprintf(stderr, "Error: malloc failed for output\n");
        return 1;
    }

    size_t output_size = 0;
    size_t pos = 0;

    /* --- Read large_f (32-bit BE) --- */
    if ((size_t)file_size < 4U) {
        /* Not enough data for large_f -> empty output */
        goto write_output;
    }

    uint32_t large_f = read_be32(&file_data[pos]);
    pos += 4U;

    /* Validate large_f */
    if (large_f == 0U || large_f > CCSDS124_MAX_PACKET_LENGTH) {
        /* Invalid large_f -> empty output (t_error = 0) */
        goto write_output;
    }

    uint32_t packet_bytes = (large_f + 7U) / 8U;

    /* --- Read M_0 --- */
    if (pos + packet_bytes > (size_t)file_size) {
        goto write_output;
    }

    /* Validate M_0 padding bits */
    if (!check_padding_zero(&file_data[pos], large_f)) {
        goto write_output;
    }

    /* Initialize compressor in manual mode */
    ccsds124_compressor_t comp;
    bitvector_t initial_mask;
    bitvector_init(&initial_mask, (size_t)large_f);
    bitvector_from_bytes(&initial_mask, &file_data[pos], (size_t)packet_bytes);
    pos += packet_bytes;

    int rc = ccsds124_compressor_init(&comp, (size_t)large_f, &initial_mask, 0, 0, 0, 0);
    if (rc != CCSDS124_OK) {
        goto write_output;
    }

    /* --- Process packets --- */
    size_t pkt_idx = 0;
    while (pos < (size_t)file_size) {
        /* Need at least 1 byte for Flag Configuration Byte */
        if (pos + 1U > (size_t)file_size) {
            break; /* Incomplete packet */
        }

        uint8_t flag_byte = file_data[pos];
        pos += 1U;

        /* Parse Flag Configuration Byte */
        /* bit7=reserved, bit6=f, bit5=p, bit4=r, bits3-0=R */
        uint8_t f_flag = (flag_byte >> 6) & 1U;
        uint8_t p_flag = (flag_byte >> 5) & 1U;
        uint8_t r_flag = (flag_byte >> 4) & 1U;
        uint8_t R_val  = flag_byte & 0x0FU;

        /* Validate R (must be 0-7) */
        if (R_val > CCSDS124_MAX_ROBUSTNESS) {
            break; /* Stop: invalid parameter */
        }

        /* CCSDS 124.0-B-1 Section 3.3.2: During initialization phase
         * (t = 0, 1, ..., R), r_t shall be 1. When R > 0, f_t shall
         * also be 1 to ensure mask synchronization. */
        if (pkt_idx <= (size_t)R_val) {
            if (r_flag == 0U) {
                break; /* Stop: non-reference packet during init phase */
            }
            if (R_val > 0U && f_flag == 0U) {
                break; /* Stop: f=0 during init phase with R > 0 */
            }
        }


        /* Need packet_bytes for packet content */
        if (pos + packet_bytes > (size_t)file_size) {
            break; /* Incomplete packet */
        }

        /* Validate packet content padding bits */
        if (!check_padding_zero(&file_data[pos], large_f)) {
            break; /* Stop: invalid padding */
        }

        /* Load packet data */
        bitvector_t input_vec;
        bitvector_init(&input_vec, (size_t)large_f);
        bitvector_from_bytes(&input_vec, &file_data[pos], (size_t)packet_bytes);
        pos += packet_bytes;

        /* Set per-packet robustness directly on compressor struct */
        comp.robustness = R_val;

        /* Set up params */
        ccsds124_params_t params;
        params.min_robustness = R_val;
        params.send_mask_flag = f_flag;
        params.new_mask_flag = p_flag;
        params.uncompressed_flag = r_flag;

        /* Compress packet */
        bitbuffer_t packet_output;
        bitbuffer_init(&packet_output);

        rc = ccsds124_compress_packet(&comp, &input_vec, &packet_output, &params);
        if (rc != CCSDS124_OK) {
            break; /* Stop on compression error */
        }

        /* Convert to bytes (byte-aligned with zero padding) */
        uint8_t packet_out_bytes[CCSDS124_MAX_OUTPUT_BYTES];
        size_t packet_out_size = bitbuffer_to_bytes(&packet_output, packet_out_bytes, sizeof(packet_out_bytes));

        /* Append to output */
        if (output_size + packet_out_size > output_capacity) {
            /* Grow output buffer */
            output_capacity = (output_size + packet_out_size) * 2U;
            uint8_t *new_buf = (uint8_t *)realloc(output_data, output_capacity);
            if (new_buf == NULL) {
                fprintf(stderr, "Error: realloc failed\n");
                free(file_data);
                free(output_data);
                return 1;
            }
            output_data = new_buf;
        }

        memcpy(&output_data[output_size], packet_out_bytes, packet_out_size);
        output_size += packet_out_size;
        pkt_idx++;
    }

write_output:
    /* Write output file */
    {
        FILE *fout = fopen(output_path, "wb");
        if (fout == NULL) {
            fprintf(stderr, "Error: cannot open %s for writing\n", output_path);
            free(file_data);
            free(output_data);
            return 1;
        }

        if (output_size > 0U) {
            fwrite(output_data, 1, output_size, fout);
        }

        fclose(fout);
    }

    free(file_data);
    free(output_data);
    return 0;
}

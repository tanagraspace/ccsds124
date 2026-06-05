/**
 * @file crossvalidation_decoder.c
 * @brief Cross-validation decoder harness for CCSDS 124.0-B-1.
 *
 * Reads a .124+config input file, decompresses each element using the
 * pocket_decompress_packet_checked() API, and writes the output in .raw+large_f
 * format (status bytes + decoded packets + final 32-bit BE packet length).
 *
 * Usage: crossvalidation_decoder <input.124+config> <output.raw+large_f>
 */

#include "pocketplus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum compressed packet length in bits per the cross-validation spec */
#define MAX_COMPRESSED_BITS 622627U

/** Read a 32-bit big-endian unsigned integer from a byte buffer. */
static uint32_t read_be32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           ((uint32_t)buf[3]);
}

/** Write a 32-bit big-endian unsigned integer to a byte buffer. */
static void write_be32(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val);
}

/**
 * Walk through the decoder input file trying each received packet
 * until F is discovered from a reference packet (rt=1).
 *
 * Two-tier: a fully-validated reference packet (strict discovery) wins.
 * If no packet validates strictly, fall back to the first truncated
 * reference packet's signaled COUNT(F) — per the cross-validation rules
 * a signaled length is to be considered even when the bitstream runs out
 * before the full I_t ("stored as the actual packet length if it is
 * valid and was not known before").
 */
static uint32_t discover_F_from_file(const uint8_t *file_data, size_t file_size,
                                     uint32_t *weak_F_out) {
    size_t pos = 0;
    uint32_t weak_F = 0;

    while (pos < file_size) {
        uint8_t reception_byte = file_data[pos];
        pos += 1U;

        if ((reception_byte & 1U) == 1U) {
            /* Type 2: lost packet — skip */
            continue;
        }

        /* Type 1: received packet */
        if (pos + 4U > file_size) {
            break; /* Truncated */
        }

        uint32_t length_bits = read_be32(&file_data[pos]);
        pos += 4U;

        if (length_bits == 0U || length_bits > MAX_COMPRESSED_BITS) {
            break; /* Invalid length — stop processing */
        }

        uint32_t length_bytes = (length_bits + 7U) / 8U;
        if (pos + length_bytes > file_size) {
            break; /* Truncated */
        }

        /* Try to discover F from this packet's bitstream */
        uint32_t F = 0;
        int rc = pocket_discover_packet_length(&file_data[pos], (size_t)length_bits, &F);
        if (rc == POCKET_OK && F > 0U) {
            return F; /* Strict discovery — found F */
        }
        if (rc == POCKET_STATUS_TRUNCATED_LENGTH && F > 0U && weak_F == 0U) {
            weak_F = F; /* Remember first signaled length; keep scanning */
        }

        /* F not found in this packet — try next received packet */
        pos += length_bytes;
    }

    /* No fully-validated reference packet: report the first signaled
     * length (if any) for the output trailer only — it is not reliable
     * enough to decode with. */
    if (weak_F_out != NULL) {
        *weak_F_out = weak_F;
    }
    return 0;
}

/**
 * Append bytes to a dynamically-growing output buffer.
 */
static int append_output(uint8_t **buf, size_t *size, size_t *cap,
                         const uint8_t *data, size_t len) {
    if (*size + len > *cap) {
        size_t new_cap = (*cap == 0U) ? 65536U : *cap * 2U;
        while (new_cap < *size + len) {
            new_cap *= 2U;
        }
        uint8_t *new_buf = (uint8_t *)realloc(*buf, new_cap);
        if (new_buf == NULL) {
            return -1;
        }
        *buf = new_buf;
        *cap = new_cap;
    }
    memcpy(*buf + *size, data, len);
    *size += len;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.124+config> <output.raw+large_f>\n", argv[0]);
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

    uint8_t *file_data = NULL;
    if (file_size > 0) {
        file_data = (uint8_t *)malloc((size_t)file_size);
        if (file_data == NULL) {
            fclose(fin);
            fprintf(stderr, "Error: malloc failed\n");
            return 1;
        }
        size_t bytes_read = fread(file_data, 1, (size_t)file_size, fin);
        if (bytes_read != (size_t)file_size) {
            free(file_data);
            fclose(fin);
            fprintf(stderr, "Error: short read\n");
            return 1;
        }
    }
    fclose(fin);

    /* Output buffer */
    uint8_t *output_data = NULL;
    size_t output_size = 0;
    size_t output_cap = 0;

    /* Discover F via pre-scan */
    uint32_t discovered_F = 0;
    uint32_t weak_F = 0;
    int F_known = 0;
    pocket_decompressor_t decomp;

    if (file_data != NULL && file_size > 0) {
        discovered_F = discover_F_from_file(file_data, (size_t)file_size, &weak_F);
        if (discovered_F > 0U && discovered_F <= POCKET_MAX_PACKET_LENGTH) {
            F_known = 1;
            pocket_decompressor_init(&decomp, (size_t)discovered_F, NULL, 0);
        }
    }

    uint32_t packet_bytes_F = F_known ? (discovered_F + 7U) / 8U : 0U;

    /* Process all elements */
    size_t pos = 0;
    int stop_processing = 0;

    while (pos < (size_t)file_size && !stop_processing) {
        /* Read Reception Byte */
        uint8_t reception_byte = file_data[pos];
        pos += 1U;

        if ((reception_byte & 1U) == 1U) {
            /* Type 2: lost packet */
            uint8_t status = 0x02;
            append_output(&output_data, &output_size, &output_cap, &status, 1);

            /* Notify decompressor of packet loss */
            if (F_known) {
                pocket_decompressor_notify_packet_loss(&decomp, 1);
            }
            continue;
        }

        /* Type 1: received packet */
        /* Read Received Packet Length (32-bit BE) */
        if (pos + 4U > (size_t)file_size) {
            /* Truncated: can't read length, don't produce output for this element */
            break;
        }

        uint32_t length_bits = read_be32(&file_data[pos]);
        pos += 4U;

        /* Validate length */
        if (length_bits == 0U || length_bits > MAX_COMPRESSED_BITS) {
            /* Invalid length: ignore this packet and stop */
            stop_processing = 1;
            continue;
        }

        uint32_t length_bytes = (length_bits + 7U) / 8U;

        /* Check if compressed bitstream can be fully read */
        if (pos + length_bytes > (size_t)file_size) {
            /* Truncated: don't produce output for this element */
            break;
        }

        const uint8_t *packet_data = &file_data[pos];
        pos += length_bytes;

        if (!F_known) {
            /* F unknown: output status 0x01 */
            uint8_t status = 0x01;
            append_output(&output_data, &output_size, &output_cap, &status, 1);
            continue;
        }

        /* Decompress with accuracy guarantee checking */
        bitvector_t output_vec;
        int rc = pocket_decompress_packet_checked(
            &decomp, packet_data, (size_t)length_bits, &output_vec, NULL);

        if (rc == POCKET_OK) {
            /* Guaranteed: write status 0x00 + decoded packet */
            uint8_t status = 0x00;
            append_output(&output_data, &output_size, &output_cap, &status, 1);

            uint8_t pkt_bytes[POCKET_MAX_PACKET_BYTES];
            memset(pkt_bytes, 0, sizeof(pkt_bytes));
            bitvector_to_bytes(&output_vec, pkt_bytes, (size_t)packet_bytes_F);
            append_output(&output_data, &output_size, &output_cap,
                         pkt_bytes, (size_t)packet_bytes_F);
        } else {
            /* Unguaranteed or error: write status 0x01 */
            uint8_t status = 0x01;
            append_output(&output_data, &output_size, &output_cap, &status, 1);
        }
    }

    /* Append final 32-bit BE packet length (0 if unknown). A signaled
     * length from a truncated reference packet counts as known for the
     * trailer even though it is not reliable enough to decode with. */
    {
        uint8_t f_bytes[4];
        write_be32(f_bytes, F_known ? discovered_F : weak_F);
        append_output(&output_data, &output_size, &output_cap, f_bytes, 4);
    }

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

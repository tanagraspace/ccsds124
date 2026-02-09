/**
 * @file crossvalidation_decoder.c
 * @brief Cross-validation decoder harness for CCSDS 124.0-B-1.
 *
 * Reads a .124+config input file, decompresses each element using the
 * pocket_decompress_packet() API, and writes the output in .raw+large_f
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

/* Maximum number of packets we'll track for status history */
#define MAX_STATUS_HISTORY 65536U

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

/** Skip COUNT values in an RLE sequence until the terminator ('10' → 0).
 *  Returns the hamming weight (number of non-terminator COUNTs decoded).
 *  Returns (uint32_t)-1 on decode error. */
static uint32_t skip_rle(bitreader_t *reader) {
    uint32_t hw = 0;
    uint32_t count_val = 0;
    int rc = pocket_count_decode(reader, &count_val);
    while (rc == POCKET_OK && count_val != 0U) {
        hw++;
        rc = pocket_count_decode(reader, &count_val);
    }
    return (rc == POCKET_OK) ? hw : (uint32_t)-1;
}

/**
 * Pre-scan a compressed packet to discover F (packet length in bits).
 *
 * Works for any time index by parsing the self-delimiting bitstream
 * structure without needing F:
 *   1. RLE(Xt) → skip COUNT values until terminator, get H(Xt)
 *   2. BIT4(Vt) → 4 bits
 *   3. If H(Xt) > 0 and Vt > 0: et (1 bit) + kt (H(Xt) bits) + ct (1 bit)
 *   4. dt (1 bit)
 *   5. If dt == 0: ft + optional mask RLE + rt
 *   6. If rt == 1: COUNT(F) reveals F
 *
 * Returns 0 if F cannot be discovered from this packet.
 */
static uint32_t prescan_discover_F(const uint8_t *data, size_t num_bits) {
    bitreader_t reader;
    bitreader_init(&reader, data, num_bits);

    /* 1. Skip RLE(Xt) — self-delimiting, doesn't need F */
    uint32_t H_Xt = skip_rle(&reader);
    if (H_Xt == (uint32_t)-1) {
        return 0;
    }

    /* 2. BIT4(Vt) — 4 bits */
    if (bitreader_remaining(&reader) < 4U) {
        return 0;
    }
    uint32_t Vt = bitreader_read_bits(&reader, 4U);

    /* 3. If H(Xt) > 0 and Vt > 0: read et, then kt+ct only if et==1 */
    if (H_Xt > 0U && Vt > 0U) {
        /* et: 1 bit */
        int et = bitreader_read_bit(&reader);
        if (et < 0) {
            return 0;
        }
        if (et == 1) {
            /* kt: H(Xt) bits — skip them one by one (may exceed 32) */
            if (bitreader_remaining(&reader) < (size_t)H_Xt) {
                return 0;
            }
            for (uint32_t i = 0U; i < H_Xt; i++) {
                bitreader_read_bit(&reader);
            }
            /* ct: 1 bit */
            if (bitreader_read_bit(&reader) < 0) {
                return 0;
            }
        }
        /* et==0: all changes are negative — no kt or ct in bitstream */
    }
    /* If H(Xt) > 0 and Vt == 0: no et/kt/ct (toggle mode) */

    /* 4. dt — 1 bit */
    int dt = bitreader_read_bit(&reader);
    if (dt < 0) {
        return 0;
    }

    if (dt == 1) {
        /* dt=1 means ft=0 and rt=0 → can't discover F from this packet */
        return 0;
    }

    /* 5. dt=0: read ft flag */
    int ft = bitreader_read_bit(&reader);
    if (ft < 0) {
        return 0;
    }

    if (ft == 1) {
        /* Full mask follows as RLE — skip it */
        if (skip_rle(&reader) == (uint32_t)-1) {
            return 0;
        }
    }

    /* Read rt flag */
    int rt = bitreader_read_bit(&reader);
    if (rt < 0) {
        return 0;
    }

    if (rt != 1) {
        /* Not a reference packet → can't discover F */
        return 0;
    }

    /* 6. rt=1: read COUNT(F) */
    uint32_t discovered_F = 0;
    int rc = pocket_count_decode(&reader, &discovered_F);
    if (rc != POCKET_OK || discovered_F == 0U) {
        return 0;
    }

    /* Validate: enough bits remaining for I_t data */
    if (bitreader_remaining(&reader) < (size_t)discovered_F) {
        return 0;
    }

    /* Validate: after I_t, at most 7 padding bits should remain */
    if ((bitreader_remaining(&reader) - (size_t)discovered_F) >= 8U) {
        return 0;
    }

    return discovered_F;
}

/**
 * Walk through the decoder input file trying each received packet
 * until F is discovered from a reference packet (rt=1).
 */
static uint32_t discover_F_from_file(const uint8_t *file_data, size_t file_size) {
    size_t pos = 0;

    while (pos < file_size) {
        uint8_t reception_byte = file_data[pos];
        pos += 1U;

        if ((reception_byte & 1U) == 1U) {
            /* Type 2: lost packet — skip */
            continue;
        }

        /* Type 1: received packet */
        if (pos + 4U > file_size) {
            return 0; /* Truncated */
        }

        uint32_t length_bits = read_be32(&file_data[pos]);
        pos += 4U;

        if (length_bits == 0U || length_bits > MAX_COMPRESSED_BITS) {
            return 0; /* Invalid length — stop processing */
        }

        uint32_t length_bytes = (length_bits + 7U) / 8U;
        if (pos + length_bytes > file_size) {
            return 0; /* Truncated */
        }

        /* Try to discover F from this packet's bitstream */
        uint32_t F = prescan_discover_F(&file_data[pos], (size_t)length_bits);
        if (F > 0U) {
            return F; /* Found F */
        }

        /* F not found in this packet — try next received packet */
        pos += length_bytes;
    }

    return 0;
}

/**
 * Parse Vt, ft, and rt from a compressed packet bitstream.
 * Uses the same skip logic as prescan_discover_F.
 *
 * @param[out] out_Vt   Parsed Vt value (0-15)
 * @param[out] out_ft   Parsed ft flag (0 or 1); 0 if dt=1
 * @param[out] out_rt   Parsed rt flag (0 or 1); -1 if not reachable
 * @return 0 on success, -1 on parse error
 */
static int parse_Vt_ft_rt(const uint8_t *data, size_t num_bits,
                           uint32_t *out_Vt, int *out_ft, int *out_rt) {
    bitreader_t reader;
    bitreader_init(&reader, data, num_bits);

    *out_Vt = 0;
    *out_ft = 0;
    *out_rt = -1;

    /* Skip RLE(Xt) */
    uint32_t H_Xt = skip_rle(&reader);
    if (H_Xt == (uint32_t)-1) {
        return -1;
    }

    /* BIT4(Vt) */
    if (bitreader_remaining(&reader) < 4U) {
        return -1;
    }
    *out_Vt = bitreader_read_bits(&reader, 4U);

    /* If H(Xt) > 0 and Vt > 0: read et, then kt+ct only if et==1 */
    if (H_Xt > 0U && *out_Vt > 0U) {
        int et = bitreader_read_bit(&reader);
        if (et < 0) return -1;
        if (et == 1) {
            if (bitreader_remaining(&reader) < (size_t)H_Xt) return -1;
            for (uint32_t i = 0U; i < H_Xt; i++) {
                bitreader_read_bit(&reader);  /* kt bits */
            }
            if (bitreader_read_bit(&reader) < 0) return -1;  /* ct */
        }
        /* et==0: no kt or ct in bitstream */
    }

    /* dt */
    int dt = bitreader_read_bit(&reader);
    if (dt < 0) return -1;

    if (dt == 1) {
        *out_ft = 0;
        *out_rt = 0;  /* dt=1 implies ft=0, rt=0 */
        return 0;
    }

    /* dt=0: read ft */
    int ft = bitreader_read_bit(&reader);
    if (ft < 0) return -1;
    *out_ft = ft;

    if (ft == 1) {
        if (skip_rle(&reader) == (uint32_t)-1) return -1;
    }

    /* read rt */
    int rt = bitreader_read_bit(&reader);
    if (rt < 0) return -1;

    *out_rt = rt;
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
    int F_known = 0;
    pocket_decompressor_t decomp;

    if (file_data != NULL && file_size > 0) {
        discovered_F = discover_F_from_file(file_data, (size_t)file_size);
        if (discovered_F > 0U && discovered_F <= POCKET_MAX_PACKET_LENGTH) {
            F_known = 1;
            pocket_decompressor_init(&decomp, (size_t)discovered_F, NULL, 0);
        }
    }

    uint32_t packet_bytes_F = F_known ? (discovered_F + 7U) / 8U : 0U;

    /* Status history for accuracy guarantee tracking */
    uint8_t *status_history = NULL;
    size_t status_count = 0;
    size_t status_cap = 0;

    /* Track mask synchronization state (v1.5 / v1.11).
     * mask_synced = 1 when the decoder's mask matches the encoder's.
     * Initially false: the encoder may start with a non-zero initial
     * mask (large_m_0) that the decoder doesn't know.
     * Goes to 0 on packet loss or any 0x01 status (state restored but
     * encoder's state moved forward).
     * Returns to 1 only when a reference packet with full mask (ft=1)
     * is received and successfully decoded. */
    int mask_synced = 0;

    /* Debug mode: set CROSSVAL_DEBUG=1 to print per-packet diagnostics */
    int debug_mode = (getenv("CROSSVAL_DEBUG") != NULL);
    int element_idx = 0;

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

            /* Track status */
            append_output(&status_history, &status_count, &status_cap, &status, 1);

            /* Notify decompressor of packet loss */
            if (F_known) {
                pocket_decompressor_notify_packet_loss(&decomp, 1);
            }
            mask_synced = 0;
            if (debug_mode) {
                fprintf(stderr, "E%d: LOST mask_synced=0\n", element_idx);
            }
            element_idx++;
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
            append_output(&status_history, &status_count, &status_cap, &status, 1);
            continue;
        }

        /* Save decompressor state before attempting decompression.
         * Per cross-validation v1.9: "The packet reference is now restored
         * in case an invalid packet is received, so as to avoid invalid
         * changes that can propagate to subsequent packets." */
        pocket_decompressor_t saved_decomp;
        memcpy(&saved_decomp, &decomp, sizeof(decomp));

        /* Attempt decompression */
        bitreader_t reader;
        bitreader_init(&reader, packet_data, (size_t)length_bits);

        bitvector_t output_vec;
        int rc = pocket_decompress_packet(&decomp, &reader, &output_vec);

        /* Validate: only padding bits should remain (at most 7) (v1.10) */
        if (rc == POCKET_OK && bitreader_remaining(&reader) >= 8U) {
            rc = POCKET_ERROR_OVERFLOW;
        }

        if (rc == POCKET_OK) {
            /* Parse Vt, ft, and rt from the bitstream to check accuracy guarantee.
             * The accuracy check uses R (base robustness), not Vt (effective
             * robustness). Vt = R + Ct where Ct counts consecutive no-change
             * packets. The accuracy check only applies to non-reference packets. */
            uint32_t Vt = 0;
            int ft = 0;
            int rt = -1;
            int parse_ok = parse_Vt_ft_rt(packet_data, (size_t)length_bits,
                                           &Vt, &ft, &rt);

            /* v1.11: Check for inconsistent delta and full mask values.
             * Only meaningful when the decoder is synchronized (mask_synced=1),
             * because an out-of-sync decoder naturally has a different delta
             * mask than the full mask (e.g., unknown initial mask large_m_0,
             * or preceding loss desynchronized the decoder).
             * When detected: output 0x01, but keep ALL decompressor state
             * since the full mask and I_t (for rt=1) provide correct values
             * for subsequent packets. */
            int mask_inconsistent_detected = (mask_synced != 0) &&
                                             (decomp.mask_inconsistent != 0U);
            int count_f_mismatch_detected = (decomp.count_f_mismatch != 0U);

            int guaranteed;
            if (mask_inconsistent_detected) {
                guaranteed = 0;
            } else if (count_f_mismatch_detected) {
                guaranteed = 0;
            } else if (parse_ok != 0) {
                guaranteed = 0;
            } else if (rt == 1) {
                /* Reference packet: per v1.5, a reference packet is only
                 * guaranteed if the mask is synchronized or the packet
                 * itself provides a full mask (ft=1) to resynchronize. */
                if (mask_synced || ft == 1) {
                    guaranteed = 1;
                } else {
                    guaranteed = 0;
                }
            } else {
                /* Non-reference packet: check preceding R received packets.
                 * R = Vt - Ct. Since Ct isn't directly available, use Vt as
                 * upper bound. Skip lost packets (0x02) in the window since
                 * the robustness guarantee applies to decoded packets only. */
                guaranteed = 1;
                if (Vt > 0U) {
                    size_t checked = 0;
                    size_t idx = status_count;
                    while (checked < (size_t)Vt && idx > 0U) {
                        idx--;
                        if (status_history[idx] == 0x02) {
                            continue; /* Skip lost packets */
                        }
                        if (status_history[idx] != 0x00) {
                            guaranteed = 0;
                            break;
                        }
                        checked++;
                    }
                    /* If we checked some but not all Vt received packets,
                     * the history is too short. In this case, trust the
                     * guarantee since early packets are inherently reliable. */
                }
            }

            if (debug_mode) {
                fprintf(stderr, "E%d: rc=OK Vt=%u ft=%d rt=%d mask_synced=%d mask_incon=%u count_f=%u guaranteed=%d\n",
                        element_idx, Vt, ft, rt, mask_synced, decomp.mask_inconsistent,
                        decomp.count_f_mismatch, guaranteed);
            }

            if (guaranteed) {
                /* Status 0x00: success */
                uint8_t status = 0x00;
                append_output(&output_data, &output_size, &output_cap, &status, 1);

                /* Write decoded packet (padded to byte boundary) */
                uint8_t pkt_bytes[POCKET_MAX_PACKET_BYTES];
                memset(pkt_bytes, 0, sizeof(pkt_bytes));
                bitvector_to_bytes(&output_vec, pkt_bytes, (size_t)packet_bytes_F);
                append_output(&output_data, &output_size, &output_cap,
                             pkt_bytes, (size_t)packet_bytes_F);

                append_output(&status_history, &status_count, &status_cap, &status, 1);

                /* Update mask sync: ft=1 resynchronizes the mask (v1.5).
                 * Any guaranteed packet with ft=1 syncs the mask,
                 * regardless of whether rt=1. */
                if (ft == 1) {
                    mask_synced = 1;
                }
            } else if (mask_inconsistent_detected) {
                /* v1.11: mask inconsistency detected while synced.
                 * The full mask (ft=1) disagrees with the delta-updated mask,
                 * indicating corruption. Restore state and output 0x01. */
                memcpy(&decomp, &saved_decomp, sizeof(decomp));
                mask_synced = 0;

                uint8_t status = 0x01;
                append_output(&output_data, &output_size, &output_cap, &status, 1);
                append_output(&status_history, &status_count, &status_cap, &status, 1);
            } else if (count_f_mismatch_detected) {
                /* COUNT(F) mismatch: the encoded packet length doesn't match
                 * expected F. Keep decompressor state (the library already
                 * processed the packet using the known F) but mark as
                 * unguaranteed since the data may be shifted/corrupt. */
                /* Keep state: mask updates and prev_output are applied */
                uint8_t status = 0x01;
                append_output(&output_data, &output_size, &output_cap, &status, 1);
                append_output(&status_history, &status_count, &status_cap, &status, 1);

                /* ft=1 still resynchronizes the mask even if unguaranteed */
                if (ft == 1) {
                    mask_synced = 1;
                }
            } else {
                /* Status 0x01: cannot guarantee accuracy.
                 * Restore decompressor state to prevent error propagation. */
                memcpy(&decomp, &saved_decomp, sizeof(decomp));
                mask_synced = 0;

                uint8_t status = 0x01;
                append_output(&output_data, &output_size, &output_cap, &status, 1);
                append_output(&status_history, &status_count, &status_cap, &status, 1);
            }
        } else {
            /* Decompression failed: restore state and output status 0x01 */
            if (debug_mode) {
                fprintf(stderr, "E%d: rc=%d (DECOMPRESS FAIL) mask_synced=%d reader_pos=%zu/%zu len_bits=%u\n",
                        element_idx, rc, mask_synced,
                        bitreader_position(&reader), (size_t)length_bits, length_bits);
            }
            memcpy(&decomp, &saved_decomp, sizeof(decomp));
            mask_synced = 0;

            uint8_t status = 0x01;
            append_output(&output_data, &output_size, &output_cap, &status, 1);
            append_output(&status_history, &status_count, &status_cap, &status, 1);
        }
        element_idx++;
    }

    /* Append final 32-bit BE packet length (0 if unknown) */
    {
        uint8_t f_bytes[4];
        write_be32(f_bytes, F_known ? discovered_F : 0U);
        append_output(&output_data, &output_size, &output_cap, f_bytes, 4);
    }

    /* Write output file */
    {
        FILE *fout = fopen(output_path, "wb");
        if (fout == NULL) {
            fprintf(stderr, "Error: cannot open %s for writing\n", output_path);
            free(file_data);
            free(output_data);
            free(status_history);
            return 1;
        }

        if (output_size > 0U) {
            fwrite(output_data, 1, output_size, fout);
        }

        fclose(fout);
    }

    free(file_data);
    free(output_data);
    free(status_history);
    return 0;
}

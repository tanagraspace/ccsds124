//! POCKET+ decompression algorithm implementation.
//!
//! Implements CCSDS 124.0-B-1 decompression (inverse of Section 5.3):
//! - Decompressor initialization and state management
//! - Packet decompression and mask reconstruction
//! - Output packet decoding: parsing hₜ || qₜ || uₜ

#![allow(clippy::cast_possible_truncation)]
#![allow(clippy::too_many_lines)]
#![allow(dead_code)]

use crate::bitreader::BitReader;
use crate::bitvector::BitVector;
use crate::decode::{bit_insert, count_decode, rle_decode};
use crate::error::PocketError;

/// POCKET+ decompressor state.
#[derive(Clone)]
pub struct Decompressor {
    /// Packet length in bits (F).
    f: usize,
    /// Robustness level (R).
    robustness: u8,
    /// Current mask vector.
    mask: BitVector,
    /// Initial mask (for reset).
    initial_mask: BitVector,
    /// Previous output vector.
    prev_output: BitVector,
    /// Positive changes tracker (Xt).
    xt: BitVector,
    /// Reusable extraction mask buffer.
    extraction_mask: BitVector,
    /// Current time step.
    t: usize,
}

impl Decompressor {
    /// Create a new decompressor.
    pub fn new(
        f: usize,
        initial_mask: Option<&BitVector>,
        robustness: u8,
    ) -> Result<Self, PocketError> {
        if f == 0 || f > 65535 {
            return Err(PocketError::InvalidPacketSize(f));
        }
        if robustness > 7 {
            return Err(PocketError::InvalidRobustness(robustness as usize));
        }

        let mask = initial_mask.cloned().unwrap_or_else(|| BitVector::new(f));
        let initial = mask.clone();

        let mut decomp = Self {
            f,
            robustness,
            mask,
            initial_mask: initial,
            prev_output: BitVector::new(f),
            xt: BitVector::new(f),
            extraction_mask: BitVector::new(f),
            t: 0,
        };

        decomp.reset();
        Ok(decomp)
    }

    /// Reset decompressor to initial state.
    pub fn reset(&mut self) {
        self.t = 0;
        self.mask.copy_from(&self.initial_mask);
        self.prev_output.zero();
        self.xt.zero();
    }

    /// Decompress a single packet.
    pub fn decompress_packet(&mut self, reader: &mut BitReader) -> Result<BitVector, PocketError> {
        let mut output = BitVector::new(self.f);

        // Copy previous output as prediction base
        output.copy_from(&self.prev_output);

        // Clear positive changes tracker
        self.xt.zero();

        // ====================================================================
        // Parse hₜ: Mask change information
        // hₜ = RLE(Xₜ) || BIT₄(Vₜ) || eₜ || kₜ || cₜ || ḋₜ
        // ====================================================================

        // Decode RLE(Xₜ) - mask changes
        let xt = rle_decode(reader, self.f)?;

        // Read BIT₄(Vₜ) - effective robustness
        let vt = reader.read_bits(4)? as u8;

        // Process eₜ, kₜ, cₜ if Vₜ > 0 and there are changes
        let mut ct = false;
        let change_count = xt.hamming_weight();

        if vt > 0 && change_count > 0 {
            // Read eₜ
            let et = reader.read_bit()? != 0;

            if et {
                // Read kₜ bits and apply mask updates directly (no allocation)
                // kₜ has one bit per change in Xt
                for i in 0..self.f {
                    if xt.get_bit(i) != 0 {
                        let kt_bit = reader.read_bit()? != 0;
                        // kt=1 means positive update (mask becomes 0)
                        // kt=0 means negative update (mask becomes 1)
                        if kt_bit {
                            self.mask.set_bit(i, 0);
                            self.xt.set_bit(i, 1); // Track positive change
                        } else {
                            self.mask.set_bit(i, 1);
                        }
                    }
                }

                // Read cₜ
                ct = reader.read_bit()? != 0;
            } else {
                // et = 0: all updates are negative (mask bits become 1)
                for i in 0..self.f {
                    if xt.get_bit(i) != 0 {
                        self.mask.set_bit(i, 1);
                    }
                }
            }
        } else if vt == 0 && change_count > 0 {
            // Vt = 0: toggle mask bits at change positions
            for i in 0..self.f {
                if xt.get_bit(i) != 0 {
                    let current_val = self.mask.get_bit(i);
                    let toggled = u8::from(current_val == 0);
                    self.mask.set_bit(i, toggled);
                }
            }
        }
        // else: No changes to apply (change_count == 0)

        // Read ḋₜ
        let dt = reader.read_bit()? != 0;

        // ====================================================================
        // Parse qₜ: Optional full mask
        // ====================================================================

        let mut rt = false;

        // dt=1 means both ft=0 and rt=0 (optimization per CCSDS Eq. 13)
        // dt=0 means we need to read ft and rt from the stream
        if !dt {
            // Read ft flag
            let ft = reader.read_bit()? != 0;

            if ft {
                // Full mask follows: decode RLE(M XOR (M<<))
                let mask_diff = rle_decode(reader, self.f)?;

                // Reverse the horizontal XOR to get the actual mask.
                // HXOR encoding: HXOR[i] = M[i] XOR M[i+1], with HXOR[F-1] = M[F-1]
                // Reversal: start from LSB (position F-1) and work towards MSB (position 0)
                // M[F-1] = HXOR[F-1] (just copy)
                // M[i] = HXOR[i] XOR M[i+1] for i < F-1

                // Copy LSB bit directly (position F-1 in bitvector)
                let mut current = mask_diff.get_bit(self.f - 1);
                self.mask.set_bit(self.f - 1, current);

                // Process remaining bits from F-2 down to 0
                for i in (0..self.f - 1).rev() {
                    let hxor_bit = mask_diff.get_bit(i);
                    // M[i] = HXOR[i] XOR M[i+1] = HXOR[i] XOR current
                    current ^= hxor_bit;
                    self.mask.set_bit(i, current);
                }
            }

            // Read rt flag
            rt = reader.read_bit()? != 0;
        }

        // ====================================================================
        // Parse uₜ: Data component
        // ====================================================================

        if rt {
            // Full packet follows: COUNT(F) || Iₜ
            let _packet_length = count_decode(reader)?;

            // Read full packet
            for i in 0..self.f {
                let bit = reader.read_bit()?;
                output.set_bit(i, bit);
            }
        } else {
            // Compressed: extract unpredictable bits
            if ct && vt > 0 {
                // BE(Iₜ, (Xₜ OR Mₜ)) - need combined mask
                self.extraction_mask.copy_from(&self.mask);
                self.extraction_mask.or_assign(&self.xt);
                bit_insert(reader, &mut output, &self.extraction_mask)?;
            } else {
                // BE(Iₜ, Mₜ) - use mask directly (no allocation)
                bit_insert(reader, &mut output, &self.mask)?;
            }
        }

        // ====================================================================
        // Update state for next cycle
        // ====================================================================

        self.prev_output.copy_from(&output);
        self.t += 1;

        Ok(output)
    }
}

/// Decompress data using POCKET+ algorithm.
///
/// # Arguments
///
/// * `data` - Compressed data bytes to decompress
/// * `packet_size` - Size of each packet in bits (must be divisible by 8)
/// * `robustness` - Robustness parameter R (0-7)
///
/// # Returns
///
/// Decompressed data as a byte vector, or an error if decompression fails.
///
/// # Errors
///
/// Returns `PocketError` if:
/// - `packet_size` is 0 or not divisible by 8
/// - `robustness` is greater than 7
/// - Compressed data is invalid or corrupted
///
/// This byte-buffer convenience API requires `packet_size` (in bits) to be a
/// multiple of 8. The CCSDS 124.0-B-1 standard itself allows any F in
/// 1..=65535 bits — use the low-level [`Decompressor`] for non-byte-aligned
/// packet lengths.
pub fn decompress(
    data: &[u8],
    packet_size: usize,
    robustness: usize,
) -> Result<Vec<u8>, PocketError> {
    // Validate parameters
    if packet_size == 0 || packet_size % 8 != 0 {
        return Err(PocketError::InvalidPacketSize(packet_size));
    }

    if robustness > 7 {
        return Err(PocketError::InvalidRobustness(robustness));
    }

    if data.is_empty() {
        return Err(PocketError::UnexpectedEndOfInput);
    }

    // Initialize decompressor
    let mut decomp = Decompressor::new(packet_size, None, robustness as u8)?;

    // Initialize bit reader
    let mut reader = BitReader::new(data, data.len() * 8);

    // Output packet size in bytes
    let packet_bytes = (packet_size + 7) / 8;
    let mut output = Vec::new();

    // Decompress packets until input exhausted
    while reader.remaining() > 0 {
        let packet = decomp.decompress_packet(&mut reader)?;

        // Convert to bytes and append
        let packet_data = packet.to_bytes();
        output.extend_from_slice(&packet_data[..packet_bytes]);

        // Align to byte boundary for next packet
        reader.align_byte();
    }

    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::compress::compress;

    #[test]
    fn test_decompress_invalid_packet_size_zero() {
        let data = vec![0u8; 10];
        let result = decompress(&data, 0, 1);
        assert!(matches!(result, Err(PocketError::InvalidPacketSize(0))));
    }

    #[test]
    fn test_decompress_invalid_packet_size_not_byte_aligned() {
        let data = vec![0u8; 10];
        let result = decompress(&data, 721, 1);
        assert!(matches!(result, Err(PocketError::InvalidPacketSize(721))));
    }

    #[test]
    fn test_decompress_invalid_robustness() {
        let data = vec![0u8; 10];
        let result = decompress(&data, 720, 8);
        assert!(matches!(result, Err(PocketError::InvalidRobustness(8))));
    }

    #[test]
    fn test_decompress_empty_input() {
        let data: Vec<u8> = vec![];
        let result = decompress(&data, 720, 1);
        assert!(matches!(result, Err(PocketError::UnexpectedEndOfInput)));
    }

    #[test]
    fn test_decompressor_new() {
        let decomp = Decompressor::new(720, None, 2);
        assert!(decomp.is_ok());
        let decomp = decomp.unwrap();
        assert_eq!(decomp.f, 720);
        assert_eq!(decomp.robustness, 2);
    }

    #[test]
    fn test_decompressor_new_invalid_f() {
        let result = Decompressor::new(0, None, 2);
        assert!(matches!(result, Err(PocketError::InvalidPacketSize(0))));

        let result = Decompressor::new(65536, None, 2);
        assert!(matches!(result, Err(PocketError::InvalidPacketSize(65536))));
    }

    #[test]
    fn test_decompressor_new_invalid_robustness() {
        let result = Decompressor::new(720, None, 8);
        assert!(matches!(result, Err(PocketError::InvalidRobustness(8))));
    }

    #[test]
    fn test_round_trip_single_packet() {
        // Create a simple test packet
        let original = vec![0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE];

        // Compress with uncompressed flag to ensure full packet is stored
        let compressed = compress(&original, 64, 1, 10, 20, 50).unwrap();

        // Decompress
        let decompressed = decompress(&compressed, 64, 1).unwrap();

        assert_eq!(decompressed, original);
    }

    #[test]
    fn test_round_trip_multiple_packets() {
        // Create test data with 2 packets of 8 bytes each (64 bits)
        let original = vec![
            0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE, // Packet 1
            0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, // Packet 2
        ];

        let compressed = compress(&original, 64, 1, 10, 20, 50).unwrap();
        let decompressed = decompress(&compressed, 64, 1).unwrap();

        assert_eq!(decompressed, original);
    }

    #[test]
    fn test_round_trip_all_zeros() {
        let original = vec![0u8; 90]; // One packet of 720 bits

        let compressed = compress(&original, 720, 2, 20, 50, 100).unwrap();
        let decompressed = decompress(&compressed, 720, 2).unwrap();

        assert_eq!(decompressed, original);
    }

    #[test]
    fn test_round_trip_all_ones() {
        let original = vec![0xFF; 90]; // One packet of 720 bits

        let compressed = compress(&original, 720, 2, 20, 50, 100).unwrap();
        let decompressed = decompress(&compressed, 720, 2).unwrap();

        assert_eq!(decompressed, original);
    }

    #[test]
    fn test_round_trip_alternating() {
        let original: Vec<u8> = (0..90)
            .map(|i| if i % 2 == 0 { 0xAA } else { 0x55 })
            .collect();

        let compressed = compress(&original, 720, 1, 10, 20, 50).unwrap();
        let decompressed = decompress(&compressed, 720, 1).unwrap();

        assert_eq!(decompressed, original);
    }

    #[test]
    fn test_round_trip_many_packets() {
        // Create 10 packets of housekeeping data
        let original: Vec<u8> = (0..900).map(|i| (i % 256) as u8).collect();

        let compressed = compress(&original, 720, 2, 20, 50, 100).unwrap();
        let decompressed = decompress(&compressed, 720, 2).unwrap();

        assert_eq!(decompressed, original);
    }
}

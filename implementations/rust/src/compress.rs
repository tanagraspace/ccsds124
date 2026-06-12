//! CCSDS 124.0-B-1 compression algorithm implementation.
//!
//! Implements CCSDS 124.0-B-1 Section 5.3 (Encoding Step):
//! - Compressor initialization and state management
//! - Main compression algorithm
//! - Output packet encoding: oₜ = hₜ || qₜ || uₜ

#![allow(clippy::cast_possible_truncation)]
#![allow(clippy::cast_possible_wrap)]
#![allow(clippy::too_many_lines)]

use crate::bitbuffer::BitBuffer;
use crate::bitvector::BitVector;
use crate::encode::{bit_extract, bit_extract_forward, count_encode, rle_encode};
use crate::error::Ccsds124Error;
use crate::mask::{compute_change, update_build, update_mask};

/// Maximum history size for robustness.
const MAX_HISTORY: usize = 16;

/// Maximum Vt history for ct calculation.
const MAX_VT_HISTORY: usize = 16;

/// Compression parameters for a single packet.
#[derive(Clone, Debug, Default)]
pub struct CompressionParams {
    /// New mask flag (ṗₜ).
    pub new_mask_flag: bool,
    /// Send full mask flag (ḟₜ).
    pub send_mask_flag: bool,
    /// Send uncompressed flag (ṙₜ).
    pub uncompressed_flag: bool,
}

/// CCSDS 124.0-B-1 compressor state.
#[derive(Clone)]
pub struct Compressor {
    /// Packet length in bits (F).
    f: usize,
    /// Robustness level (R).
    robustness: u8,
    /// Current mask vector.
    mask: BitVector,
    /// Previous mask vector.
    prev_mask: BitVector,
    /// Build vector.
    build: BitVector,
    /// Previous input vector.
    prev_input: BitVector,
    /// Initial mask (for reset).
    initial_mask: BitVector,
    /// Change history (circular buffer).
    change_history: Vec<BitVector>,
    /// Current history index.
    history_index: usize,
    /// New mask flag history.
    flag_history: Vec<bool>,
    /// Flag history index.
    flag_history_index: usize,
    /// Current time step.
    t: usize,
    /// Pt limit (new mask interval).
    pt_limit: usize,
    /// Ft limit (send mask interval).
    ft_limit: usize,
    /// Rt limit (uncompressed interval).
    rt_limit: usize,
    /// Pt counter.
    pt_counter: usize,
    /// Ft counter.
    ft_counter: usize,
    /// Rt counter.
    rt_counter: usize,
}

impl Compressor {
    /// Create a new compressor.
    pub fn new(
        f: usize,
        initial_mask: Option<&BitVector>,
        robustness: u8,
        pt_limit: usize,
        ft_limit: usize,
        rt_limit: usize,
    ) -> Result<Self, Ccsds124Error> {
        if f == 0 || f > 65535 {
            return Err(Ccsds124Error::InvalidPacketSize(f));
        }
        if robustness > 7 {
            return Err(Ccsds124Error::InvalidRobustness(robustness as usize));
        }

        let mask = initial_mask.cloned().unwrap_or_else(|| BitVector::new(f));
        let initial = mask.clone();

        let mut change_history = Vec::with_capacity(MAX_HISTORY);
        for _ in 0..MAX_HISTORY {
            let mut v = BitVector::new(f);
            v.zero();
            change_history.push(v);
        }

        let mut comp = Self {
            f,
            robustness,
            mask,
            prev_mask: BitVector::new(f),
            build: BitVector::new(f),
            prev_input: BitVector::new(f),
            initial_mask: initial,
            change_history,
            history_index: 0,
            flag_history: vec![false; MAX_VT_HISTORY],
            flag_history_index: 0,
            t: 0,
            pt_limit,
            ft_limit,
            rt_limit,
            pt_counter: pt_limit,
            ft_counter: ft_limit,
            rt_counter: rt_limit,
        };

        comp.reset();
        Ok(comp)
    }

    /// Reset compressor to initial state.
    pub fn reset(&mut self) {
        self.t = 0;
        self.history_index = 0;
        self.flag_history_index = 0;

        self.mask.copy_from(&self.initial_mask);
        self.prev_mask.zero();
        self.build.zero();
        self.prev_input.zero();

        for change in &mut self.change_history {
            change.zero();
        }
        for flag in &mut self.flag_history {
            *flag = false;
        }

        self.pt_counter = self.pt_limit;
        self.ft_counter = self.ft_limit;
        self.rt_counter = self.rt_limit;
    }

    /// Compute robustness window Xₜ.
    fn compute_robustness_window(&self, current_change: &BitVector) -> BitVector {
        if self.robustness == 0 || self.t == 0 {
            current_change.clone()
        } else {
            let mut xt = current_change.clone();
            let num_changes = self.t.min(self.robustness as usize);

            for i in 1..=num_changes {
                let hist_idx = (self.history_index + MAX_HISTORY - i) % MAX_HISTORY;
                let new_xt = xt.or(&self.change_history[hist_idx]);
                xt.copy_from(&new_xt);
            }
            xt
        }
    }

    /// Compute effective robustness Vₜ.
    fn compute_effective_robustness(&self) -> u8 {
        let rt = self.robustness;
        let mut vt = rt;

        if self.t > rt as usize {
            let mut ct = 0u8;
            for i in (rt as usize + 1)..=15.min(self.t) {
                let hist_idx = (self.history_index + MAX_HISTORY - i) % MAX_HISTORY;
                if self.change_history[hist_idx].hamming_weight() > 0 {
                    break;
                }
                ct += 1;
                if ct >= 15 - rt {
                    break;
                }
            }
            vt = (rt + ct).min(15);
        }
        vt
    }

    /// Check if there are positive updates.
    fn has_positive_updates(&self, xt: &BitVector) -> bool {
        let inverted = self.mask.not();
        let positive = xt.and(&inverted);
        positive.hamming_weight() > 0
    }

    /// Compute cₜ flag.
    fn compute_ct_flag(&self, vt: u8, current_new_mask_flag: bool) -> bool {
        if vt == 0 {
            return false;
        }

        let mut count = i32::from(current_new_mask_flag);
        let iterations = (vt as usize).min(self.t);

        for i in 0..iterations {
            let hist_idx = (self.flag_history_index + MAX_VT_HISTORY - 1 - i) % MAX_VT_HISTORY;
            if self.flag_history[hist_idx] {
                count += 1;
            }
        }

        count >= 2
    }

    /// Compress a single packet.
    pub fn compress_packet(
        &mut self,
        input: &BitVector,
        params: &CompressionParams,
    ) -> Result<BitBuffer, Ccsds124Error> {
        if input.len() != self.f {
            return Err(Ccsds124Error::InvalidInputLength {
                expected: self.f,
                actual: input.len(),
            });
        }

        let mut output = BitBuffer::new();

        // Step 1: Update mask and build vectors
        self.prev_mask.copy_from(&self.mask);
        let prev_build = self.build.clone();

        if self.t > 0 {
            update_build(
                &mut self.build,
                input,
                &self.prev_input,
                params.new_mask_flag,
                self.t,
            );
            update_mask(
                &mut self.mask,
                input,
                &self.prev_input,
                &prev_build,
                params.new_mask_flag,
            );
        }

        let change = compute_change(&self.mask, &self.prev_mask, self.t);
        self.change_history[self.history_index].copy_from(&change);

        // Step 2: Encode output packet
        let xt = self.compute_robustness_window(&change);
        let vt = self.compute_effective_robustness();
        let dt = u8::from(!params.send_mask_flag && !params.uncompressed_flag);

        // Component hₜ: RLE(Xₜ) || BIT₄(Vₜ) || eₜ || kₜ || cₜ || ḋₜ
        rle_encode(&mut output, &xt)?;
        output.append_value(u32::from(vt), 4);

        if vt > 0 && xt.hamming_weight() > 0 {
            let et = self.has_positive_updates(&xt);
            output.append_bit(u8::from(et));

            if et {
                let inverted = self.mask.not();
                bit_extract_forward(&mut output, &inverted, &xt)?;

                let ct = self.compute_ct_flag(vt, params.new_mask_flag);
                output.append_bit(u8::from(ct));
            }
        }

        output.append_bit(dt);

        // Component qₜ
        if dt == 0 {
            if params.send_mask_flag {
                output.append_bit(1);
                let shifted = self.mask.left_shift();
                let diff = self.mask.xor(&shifted);
                rle_encode(&mut output, &diff)?;
            } else {
                output.append_bit(0);
            }
        }

        // Component uₜ
        if params.uncompressed_flag {
            output.append_bit(1);
            count_encode(&mut output, self.f as u32)?;
            output.append_bitvector(input);
        } else {
            if dt == 0 {
                output.append_bit(0);
            }

            let ct = self.compute_ct_flag(vt, params.new_mask_flag);
            if ct && vt > 0 {
                let extraction_mask = self.mask.or(&xt);
                bit_extract(&mut output, input, &extraction_mask)?;
            } else {
                bit_extract(&mut output, input, &self.mask)?;
            }
        }

        // Step 3: Update state
        self.prev_input.copy_from(input);
        self.prev_mask.copy_from(&self.mask);
        self.flag_history[self.flag_history_index] = params.new_mask_flag;
        self.flag_history_index = (self.flag_history_index + 1) % MAX_VT_HISTORY;
        self.t += 1;
        self.history_index = (self.history_index + 1) % MAX_HISTORY;

        Ok(output)
    }
}

/// Compress multiple packets of housekeeping data.
///
/// This byte-buffer convenience API requires `packet_size` (in bits) to be a
/// multiple of 8 so packets map cleanly onto the input byte slice. The CCSDS
/// 124.0-B-1 standard itself allows any F in 1..=65535 bits — use the
/// low-level [`Compressor`] for non-byte-aligned packet lengths.
pub fn compress(
    data: &[u8],
    packet_size: usize,
    robustness: usize,
    pt_limit: usize,
    ft_limit: usize,
    rt_limit: usize,
) -> Result<Vec<u8>, Ccsds124Error> {
    if packet_size == 0 {
        return Err(Ccsds124Error::InvalidPacketSize(packet_size));
    }
    if packet_size % 8 != 0 {
        return Err(Ccsds124Error::InvalidPacketSize(packet_size));
    }
    if robustness > 7 {
        return Err(Ccsds124Error::InvalidRobustness(robustness));
    }

    let packet_bytes = packet_size / 8;
    if data.is_empty() {
        return Ok(Vec::new());
    }
    if data.len() % packet_bytes != 0 {
        return Err(Ccsds124Error::InvalidInputLength {
            expected: (data.len() / packet_bytes + 1) * packet_bytes,
            actual: data.len(),
        });
    }

    let num_packets = data.len() / packet_bytes;
    let mut comp = Compressor::new(
        packet_size,
        None,
        robustness as u8,
        pt_limit,
        ft_limit,
        rt_limit,
    )?;

    let mut output = Vec::new();

    for i in 0..num_packets {
        let packet_data = &data[i * packet_bytes..(i + 1) * packet_bytes];
        let input = BitVector::from_bytes(packet_data, packet_size);

        let params = if pt_limit > 0 && ft_limit > 0 && rt_limit > 0 {
            if i == 0 {
                CompressionParams {
                    new_mask_flag: false,
                    send_mask_flag: true,
                    uncompressed_flag: true,
                }
            } else {
                let send_mask_flag = if comp.ft_counter == 1 {
                    comp.ft_counter = ft_limit;
                    true
                } else {
                    comp.ft_counter -= 1;
                    false
                };

                let new_mask_flag = if comp.pt_counter == 1 {
                    comp.pt_counter = pt_limit;
                    true
                } else {
                    comp.pt_counter -= 1;
                    false
                };

                let uncompressed_flag = if comp.rt_counter == 1 {
                    comp.rt_counter = rt_limit;
                    true
                } else {
                    comp.rt_counter -= 1;
                    false
                };

                let (send_mask_flag, uncompressed_flag, new_mask_flag) = if i <= robustness {
                    (true, true, false)
                } else {
                    (send_mask_flag, uncompressed_flag, new_mask_flag)
                };

                CompressionParams {
                    new_mask_flag,
                    send_mask_flag,
                    uncompressed_flag,
                }
            }
        } else {
            CompressionParams {
                new_mask_flag: false,
                send_mask_flag: false,
                uncompressed_flag: false,
            }
        };

        let packet_output = comp.compress_packet(&input, &params)?;
        output.extend(packet_output.to_bytes());
    }

    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_compress_empty_input() {
        let result = compress(&[], 720, 1, 10, 20, 50);
        assert!(result.is_ok());
        assert_eq!(result.unwrap().len(), 0);
    }

    #[test]
    fn test_compress_invalid_packet_size_zero() {
        let data = vec![0u8; 90];
        let result = compress(&data, 0, 1, 10, 20, 50);
        assert!(matches!(result, Err(Ccsds124Error::InvalidPacketSize(0))));
    }

    #[test]
    fn test_compress_invalid_packet_size_not_byte_aligned() {
        let data = vec![0u8; 90];
        let result = compress(&data, 719, 1, 10, 20, 50);
        assert!(matches!(result, Err(Ccsds124Error::InvalidPacketSize(719))));
    }

    #[test]
    fn test_compress_invalid_robustness() {
        let data = vec![0u8; 90];
        let result = compress(&data, 720, 8, 10, 20, 50);
        assert!(matches!(result, Err(Ccsds124Error::InvalidRobustness(8))));
    }

    #[test]
    fn test_compress_valid_params() {
        let data = vec![0u8; 90];
        let result = compress(&data, 720, 1, 10, 20, 50);
        assert!(result.is_ok());
        let compressed = result.unwrap();
        assert!(!compressed.is_empty());
    }

    #[test]
    fn test_compressor_new() {
        let comp = Compressor::new(720, None, 2, 10, 20, 50);
        assert!(comp.is_ok());
        let comp = comp.unwrap();
        assert_eq!(comp.f, 720);
        assert_eq!(comp.robustness, 2);
    }

    #[test]
    fn test_compress_single_packet() {
        let mut comp = Compressor::new(64, None, 1, 10, 20, 50).unwrap();
        let input = BitVector::from_bytes(&[0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE], 64);

        let params = CompressionParams {
            new_mask_flag: false,
            send_mask_flag: true,
            uncompressed_flag: true,
        };

        let result = comp.compress_packet(&input, &params);
        assert!(result.is_ok());
    }

    #[test]
    fn test_compress_multiple_packets() {
        let data = vec![
            0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC,
            0xDE, 0xF0,
        ];

        let result = compress(&data, 64, 1, 10, 20, 50);
        assert!(result.is_ok());
        let compressed = result.unwrap();
        assert!(!compressed.is_empty());
    }
}

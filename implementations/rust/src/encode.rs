//! CCSDS 124.0-B-1 encoding functions (COUNT, RLE, BE).
//!
//! Implements CCSDS 124.0-B-1 Section 5.2 encoding schemes:
//! - Counter Encoding (COUNT) - Section 5.2.2, Table 5-1, Equation 9
//! - Run-Length Encoding (RLE) - Section 5.2.3, Equation 10
//! - Bit Extraction (BE) - Section 5.2.4, Equation 11

#![allow(clippy::cast_possible_truncation)]
#![allow(clippy::cast_possible_wrap)]
#![allow(clippy::cast_sign_loss)]

use crate::bitbuffer::BitBuffer;
use crate::bitvector::BitVector;
use crate::error::Ccsds124Error;

/// Pre-computed COUNT encodings for values 1-33.
///
/// - A=1: '0' (1 bit) - handled separately
/// - A=2-33: '110' || BIT5(A-2) (8 bits) = 0xC0 | (A-2)
const COUNT_VALUES: [u8; 34] = [
    0, 0, // 0: unused, 1: '0'
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, // 2-9
    0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, // 10-17
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, // 18-25
    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, // 26-33
];

/// `DeBruijn` lookup table for fast LSB position finding.
const DEBRUIJN_LOOKUP: [u32; 32] = [
    1, 2, 29, 3, 30, 15, 25, 4, 31, 23, 21, 16, 26, 18, 5, 9, 32, 28, 14, 24, 22, 20, 17, 8, 27,
    13, 19, 7, 12, 6, 11, 10,
];

/// Counter Encoding (COUNT) - CCSDS Section 5.2.2.
///
/// Encodes positive integers 1 ≤ A ≤ 65535:
/// - A = 1 → '0'
/// - 2 ≤ A ≤ 33 → '110' || BIT5(A-2)
/// - A ≥ 34 → '111' || BIT_E(A-2) where E = 2⌊log₂(A-2)+1⌋ - 6
///
/// # Arguments
/// * `output` - Bit buffer to append encoded bits to
/// * `a` - Value to encode (1-65535)
///
/// # Returns
/// `Ok(())` on success, error if value out of range or buffer overflow.
pub fn count_encode(output: &mut BitBuffer, a: u32) -> Result<(), Ccsds124Error> {
    if a == 0 || a > 65535 {
        return Err(Ccsds124Error::InvalidFormat(
            "COUNT value out of range".into(),
        ));
    }

    if a == 1 {
        // Case 1: A = 1 → '0'
        if !output.append_bit(0) {
            return Err(Ccsds124Error::BufferOverflow);
        }
    } else if a <= 33 {
        // Case 2: 2 ≤ A ≤ 33 → '110' || BIT5(A-2)
        // Use pre-computed lookup table
        if !output.append_value(u32::from(COUNT_VALUES[a as usize]), 8) {
            return Err(Ccsds124Error::BufferOverflow);
        }
    } else {
        // Case 3: A ≥ 34 → '111' || BIT_E(A-2)
        // Append '111' prefix
        if !output.append_value(0b111, 3) {
            return Err(Ccsds124Error::BufferOverflow);
        }

        // Calculate E = 2⌊log₂(A-2)+1⌋ - 6
        let value = a - 2;
        let highest_bit = 31 - value.leading_zeros() as i32;
        let e = (2 * (highest_bit + 1)) - 6;

        // Append BIT_E(A-2)
        if !output.append_value(value, e as usize) {
            return Err(Ccsds124Error::BufferOverflow);
        }
    }

    Ok(())
}

/// Run-Length Encoding (RLE) - CCSDS Section 5.2.3.
///
/// RLE(a) = COUNT(C₀) || COUNT(C₁) || ... || COUNT(C_{H(a)-1}) || '10'
///
/// where Cᵢ = 1 + (count of consecutive '0' bits before i-th '1' bit)
/// and H(a) = Hamming weight (number of '1' bits in a)
///
/// Trailing zeros are not encoded (deducible from vector length).
///
/// # Arguments
/// * `output` - Bit buffer to append encoded bits to
/// * `input` - Bit vector to encode
///
/// # Returns
/// `Ok(())` on success, error if buffer overflow.
pub fn rle_encode(output: &mut BitBuffer, input: &BitVector) -> Result<(), Ccsds124Error> {
    // Start from the end of the vector
    let mut old_bit_position = input.len() as i32;

    // Get the raw 32-bit word data
    let words = input.words();
    let num_words = words.len();

    // Process words in reverse order (from high to low)
    for word_idx in (0..num_words).rev() {
        let mut word_data = words[word_idx];

        // Process all set bits in this word
        while word_data != 0 {
            // Isolate the LSB: x = word & -word
            let lsb = word_data & word_data.wrapping_neg();

            // Find LSB position using DeBruijn sequence
            let debruijn_index = (lsb.wrapping_mul(0x077C_B531)) >> 27;
            let mut bit_position_in_word = DEBRUIJN_LOOKUP[debruijn_index as usize] as i32;

            // Count from the other side
            bit_position_in_word = 32 - bit_position_in_word;

            // Calculate global bit position
            let new_bit_position = (word_idx as i32 * 32) + bit_position_in_word;

            // Calculate delta (number of zeros + 1)
            let delta = old_bit_position - new_bit_position;

            // Encode the count
            count_encode(output, delta as u32)?;

            // Update old position for next iteration
            old_bit_position = new_bit_position;

            // Clear the processed bit
            word_data ^= lsb;
        }
    }

    // Append terminator '10'
    if !output.append_value(0b10, 2) {
        return Err(Ccsds124Error::BufferOverflow);
    }

    Ok(())
}

/// Bit Extraction (BE) - CCSDS Section 5.2.4.
///
/// BE(a, b) = a_{g_{H(b)-1}} || ... || a_{g₁} || a_{g₀}
///
/// Extracts bits from 'data' at positions where 'mask' has '1' bits.
/// Output order: MSB to LSB (reverse order of finding '1' bits).
///
/// # Arguments
/// * `output` - Bit buffer to append extracted bits to
/// * `data` - Source bit vector
/// * `mask` - Mask indicating which bits to extract
///
/// # Returns
/// `Ok(())` on success, error if length mismatch or buffer overflow.
pub fn bit_extract(
    output: &mut BitBuffer,
    data: &BitVector,
    mask: &BitVector,
) -> Result<(), Ccsds124Error> {
    if data.len() != mask.len() {
        return Err(Ccsds124Error::InvalidInputLength {
            expected: mask.len(),
            actual: data.len(),
        });
    }

    let data_words = data.words();
    let mask_words = mask.words();
    let num_words = mask_words.len();

    // Process words in REVERSE order (high to low) like RLE.
    // This gives bits from highest position to lowest.
    for word_idx in (0..num_words).rev() {
        let mut mask_word = mask_words[word_idx];
        let data_word = data_words[word_idx];

        while mask_word != 0 {
            // Isolate LSB
            let lsb = mask_word & mask_word.wrapping_neg();

            // Find LSB position using DeBruijn
            let debruijn_index = (lsb.wrapping_mul(0x077C_B531)) >> 27;
            let bit_pos_in_word = 32 - DEBRUIJN_LOOKUP[debruijn_index as usize] as i32;

            // Check if this bit is within the valid length
            let global_pos = (word_idx as i32 * 32) + bit_pos_in_word;
            if (global_pos as usize) < data.len() {
                // Extract and output data bit
                let bit = u8::from((data_word & lsb) != 0);
                if !output.append_bit(bit) {
                    return Err(Ccsds124Error::BufferOverflow);
                }
            }

            // Clear processed bit
            mask_word ^= lsb;
        }
    }

    Ok(())
}

/// Forward Bit Extraction - CCSDS Section 5.2.4 (forward order variant).
///
/// Similar to `bit_extract` but outputs bits in forward order (LSB to MSB).
///
/// # Arguments
/// * `output` - Bit buffer to append extracted bits to
/// * `data` - Source bit vector
/// * `mask` - Mask indicating which bits to extract
///
/// # Returns
/// `Ok(())` on success, error if length mismatch or buffer overflow.
pub fn bit_extract_forward(
    output: &mut BitBuffer,
    data: &BitVector,
    mask: &BitVector,
) -> Result<(), Ccsds124Error> {
    if data.len() != mask.len() {
        return Err(Ccsds124Error::InvalidInputLength {
            expected: mask.len(),
            actual: data.len(),
        });
    }

    let data_words = data.words();
    let mask_words = mask.words();
    let num_words = mask_words.len();

    // Process words in FORWARD order (low to high).
    // Within each word, find MSBs first using clz.
    for word_idx in 0..num_words {
        let mut mask_word = mask_words[word_idx];
        let data_word = data_words[word_idx];

        while mask_word != 0 {
            // Find MSB position using count leading zeros
            let clz = mask_word.leading_zeros();
            let bit_pos_in_word = clz;

            // MSB-first: physical position 0 = bit index 0
            let global_pos = (word_idx * 32) + bit_pos_in_word as usize;

            if global_pos < data.len() {
                // Extract data bit at this position
                let bit_mask = 1u32 << (31 - clz);
                let bit = u8::from((data_word & bit_mask) != 0);
                if !output.append_bit(bit) {
                    return Err(Ccsds124Error::BufferOverflow);
                }
            }

            // Clear the MSB we just processed
            mask_word &= !(1u32 << (31 - clz));
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_count_encode_one() {
        let mut output = BitBuffer::new();
        count_encode(&mut output, 1).unwrap();

        // A = 1 → '0' (1 bit)
        assert_eq!(output.len(), 1);
        let bytes = output.to_bytes();
        assert_eq!(bytes[0] & 0x80, 0x00); // First bit is 0
    }

    #[test]
    fn test_count_encode_small() {
        // A = 2 → '110' || BIT5(0) = 11000000 = 0xC0
        let mut output = BitBuffer::new();
        count_encode(&mut output, 2).unwrap();
        assert_eq!(output.len(), 8);
        assert_eq!(output.to_bytes()[0], 0xC0);

        // A = 10 → '110' || BIT5(8) = 11001000 = 0xC8
        let mut output = BitBuffer::new();
        count_encode(&mut output, 10).unwrap();
        assert_eq!(output.len(), 8);
        assert_eq!(output.to_bytes()[0], 0xC8);

        // A = 33 → '110' || BIT5(31) = 11011111 = 0xDF
        let mut output = BitBuffer::new();
        count_encode(&mut output, 33).unwrap();
        assert_eq!(output.len(), 8);
        assert_eq!(output.to_bytes()[0], 0xDF);
    }

    #[test]
    fn test_count_encode_large() {
        // A = 34 → '111' || BIT_E(32) where E = 2*6 - 6 = 6
        // 32 in 6 bits = 100000
        // Full: 111100000 = 9 bits
        let mut output = BitBuffer::new();
        count_encode(&mut output, 34).unwrap();
        assert_eq!(output.len(), 9);
    }

    #[test]
    fn test_count_encode_invalid() {
        let mut output = BitBuffer::new();
        assert!(count_encode(&mut output, 0).is_err());
        assert!(count_encode(&mut output, 65536).is_err());
    }

    #[test]
    fn test_rle_encode_empty() {
        // All zeros - just terminator '10'
        let input = BitVector::new(8);
        let mut output = BitBuffer::new();
        rle_encode(&mut output, &input).unwrap();
        assert_eq!(output.len(), 2); // Just '10'
    }

    #[test]
    fn test_rle_encode_single_bit() {
        // Single bit at position 7 (LSB of first byte)
        let mut input = BitVector::new(8);
        input.set_bit(7, 1);

        let mut output = BitBuffer::new();
        rle_encode(&mut output, &input).unwrap();

        // COUNT(1) = '0', then '10' terminator
        // Total: 3 bits
        assert_eq!(output.len(), 3);
    }

    #[test]
    fn test_bit_extract() {
        // Data: 10110011
        // Mask: 01001010
        // Extract bits at positions 1, 4, 6
        // Result: bit[1]=0, bit[4]=0, bit[6]=1 → 001

        let data = BitVector::from_bytes(&[0b1011_0011], 8);
        let mask = BitVector::from_bytes(&[0b0100_1010], 8);

        let mut output = BitBuffer::new();
        bit_extract(&mut output, &data, &mask).unwrap();

        assert_eq!(output.len(), 3);
    }

    #[test]
    fn test_bit_extract_all_ones() {
        // Extract all bits when mask is all ones
        // BE outputs from highest to lowest mask position, reversing the bits
        // 0xAB = 10101011, reversed = 11010101 = 0xD5
        let data = BitVector::from_bytes(&[0xAB], 8);
        let mask = BitVector::from_bytes(&[0xFF], 8);

        let mut output = BitBuffer::new();
        bit_extract(&mut output, &data, &mask).unwrap();

        assert_eq!(output.len(), 8);
        let bytes = output.to_bytes();
        assert_eq!(bytes[0], 0xD5); // Reversed due to BE ordering
    }

    #[test]
    fn test_bit_extract_none() {
        // Extract no bits when mask is all zeros
        let data = BitVector::from_bytes(&[0xAB], 8);
        let mask = BitVector::from_bytes(&[0x00], 8);

        let mut output = BitBuffer::new();
        bit_extract(&mut output, &data, &mask).unwrap();

        assert_eq!(output.len(), 0);
    }

    #[test]
    fn test_bit_extract_length_mismatch() {
        let data = BitVector::new(8);
        let mask = BitVector::new(16);

        let mut output = BitBuffer::new();
        assert!(bit_extract(&mut output, &data, &mask).is_err());
    }

    #[test]
    fn test_bit_extract_forward() {
        // Forward order: outputs bits in natural order (LSB to MSB)
        // For all-ones mask, this should give the original data
        let data = BitVector::from_bytes(&[0xAB], 8);
        let mask = BitVector::from_bytes(&[0xFF], 8);

        let mut output = BitBuffer::new();
        bit_extract_forward(&mut output, &data, &mask).unwrap();

        assert_eq!(output.len(), 8);
        let bytes = output.to_bytes();
        // Forward extraction preserves original order
        assert_eq!(bytes[0], 0xAB);
    }

    #[test]
    fn test_rle_encode_pattern() {
        // Pattern: 10000001 (bits at positions 0 and 7)
        let mut input = BitVector::new(8);
        input.set_bit(0, 1);
        input.set_bit(7, 1);

        let mut output = BitBuffer::new();
        rle_encode(&mut output, &input).unwrap();

        // Should encode delta=1 for last bit, delta=7 for first bit
        // COUNT(1) = '0', COUNT(7) = '110' || BIT5(5) = 11000101
        // Then '10' terminator
        // Total: 1 + 8 + 2 = 11 bits
        assert_eq!(output.len(), 11);
    }
}

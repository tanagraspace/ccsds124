//! POCKET+ decoding functions (COUNT, RLE, Bit Insert).
//!
//! Implements CCSDS 124.0-B-1 decoding (inverse of Section 5.2):
//! - Counter Decoding - inverse of COUNT encoding
//! - Run-Length Decoding - inverse of RLE encoding
//! - Bit Insertion - inverse of BE extraction

use crate::bitreader::BitReader;
use crate::bitvector::BitVector;
use crate::error::PocketError;

/// Counter Decoding - inverse of COUNT encoding.
///
/// Decodes COUNT-encoded values:
/// - '0' → 1
/// - '10' → 0 (terminator)
/// - '110' + 5 bits → value + 2
/// - '111' + variable bits → value + 2
///
/// # Arguments
/// * `reader` - Bit reader to read encoded bits from
///
/// # Returns
/// Decoded value, or error if invalid encoding.
#[inline]
pub fn count_decode(reader: &mut BitReader) -> Result<u32, PocketError> {
    // Read first bit
    let bit0 = reader.read_bit()?;

    if bit0 == 0 {
        // Case 1: '0' → value is 1
        return Ok(1);
    }

    // First bit is 1, read second bit
    let bit1 = reader.read_bit()?;

    if bit1 == 0 {
        // Case 2: '10' → terminator (value 0)
        return Ok(0);
    }

    // First two bits are 11, read third bit
    let bit2 = reader.read_bit()?;

    if bit2 == 0 {
        // Case 3: '110' + 5 bits → value + 2
        let raw = reader.read_bits(5)?;
        return Ok(raw + 2);
    }

    // Case 4: '111' + variable bits
    // Count zeros to determine field size
    let mut size = 0usize;
    loop {
        let next_bit = reader.read_bit()?;
        size += 1;
        if next_bit == 1 {
            break;
        }
    }

    // Size of value field is size + 5
    let value_bits = size + 5;

    // Back up one bit since the '1' is part of the value
    reader.back()?;

    // Read the value field
    let raw = reader.read_bits(value_bits)?;
    Ok(raw + 2)
}

/// Run-Length Decoding - inverse of RLE encoding.
///
/// Decodes RLE-encoded bit vectors by reading COUNT values until terminator.
///
/// # Arguments
/// * `reader` - Bit reader to read encoded bits from
/// * `length` - Expected length of decoded bit vector
///
/// # Returns
/// Decoded bit vector, or error if invalid encoding.
#[inline]
pub fn rle_decode(reader: &mut BitReader, length: usize) -> Result<BitVector, PocketError> {
    // Initialize result to all zeros (BitVector::new already zeroes)
    let mut result = BitVector::new(length);

    // Start from end of vector (matching RLE encoding which processes LSB to MSB)
    let mut bit_position = length;

    // Read COUNT values until terminator
    let mut delta = count_decode(reader)?;

    while delta != 0 {
        // Delta represents (count of zeros + 1). A delta beyond the
        // remaining bit position means the encoding is invalid for this
        // vector length (GOTCHAS #21): reject it instead of silently
        // skipping.
        if (delta as usize) > bit_position {
            return Err(PocketError::InvalidFormat(
                "invalid RLE delta: exceeds remaining bit position".to_string(),
            ));
        }
        bit_position -= delta as usize;
        // Set the bit at this position
        result.set_bit(bit_position, 1);

        // Read next delta
        delta = count_decode(reader)?;
    }

    Ok(result)
}

/// Bit Insertion - inverse of BE extraction.
///
/// Inserts bits from reader into data at positions where mask has '1' bits.
/// Bits are inserted in reverse order (matching BE extraction order).
///
/// # Arguments
/// * `reader` - Bit reader to read bits from
/// * `data` - Bit vector to insert bits into
/// * `mask` - Mask indicating where to insert bits
///
/// # Returns
/// `Ok(())` on success, or error if not enough bits.
#[inline]
pub fn bit_insert(
    reader: &mut BitReader,
    data: &mut BitVector,
    mask: &BitVector,
) -> Result<(), PocketError> {
    if data.len() != mask.len() {
        return Err(PocketError::InvalidInputLength {
            expected: mask.len(),
            actual: data.len(),
        });
    }

    // Insert bits in reverse order (matching BE extraction)
    // Iterate backward through mask positions to avoid Vec allocation
    let len = mask.len();
    for i in (0..len).rev() {
        if mask.get_bit(i) != 0 {
            let bit = reader.read_bit()?;
            data.set_bit(i, bit);
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    /// GOTCHAS #21 / issue #92: an RLE delta exceeding the remaining bit
    /// position must be rejected, not silently skipped.
    #[test]
    fn test_rle_decode_rejects_invalid_delta() {
        // COUNT(9) = '110'+BIT5(7), then terminator '10' -> 0xC7 0x80.
        // Delta 9 exceeds an 8-bit vector.
        let data = vec![0xC7, 0x80];
        let mut reader = BitReader::new(&data, 16);
        assert!(rle_decode(&mut reader, 8).is_err());
    }

    #[test]
    fn test_count_decode_one() {
        // '0' → 1
        let data = vec![0b0000_0000];
        let mut reader = BitReader::new(&data, 1);
        assert_eq!(count_decode(&mut reader).unwrap(), 1);
    }

    #[test]
    fn test_count_decode_terminator() {
        // '10' → 0
        let data = vec![0b1000_0000];
        let mut reader = BitReader::new(&data, 2);
        assert_eq!(count_decode(&mut reader).unwrap(), 0);
    }

    #[test]
    fn test_count_decode_small() {
        // '110' + 00000 → 2
        // 11000000 = 0xC0
        let data = vec![0xC0];
        let mut reader = BitReader::new(&data, 8);
        assert_eq!(count_decode(&mut reader).unwrap(), 2);

        // '110' + 00101 → 7
        // 11000101 = 0xC5
        let data = vec![0xC5];
        let mut reader = BitReader::new(&data, 8);
        assert_eq!(count_decode(&mut reader).unwrap(), 7);

        // '110' + 11111 → 33
        // 11011111 = 0xDF
        let data = vec![0xDF];
        let mut reader = BitReader::new(&data, 8);
        assert_eq!(count_decode(&mut reader).unwrap(), 33);
    }

    #[test]
    fn test_count_decode_large() {
        // '111' + 6-bit value (E = 6 for A = 34)
        // Value 34 = 32 + 2, so raw = 32 = 0b100000
        // Encoding: 111 + 100000 = 111100000 (9 bits)
        // Byte-aligned: 11110000 0... = 0xF0 0x00
        let data = vec![0xF0, 0x00];
        let mut reader = BitReader::new(&data, 9);
        assert_eq!(count_decode(&mut reader).unwrap(), 34);
    }

    #[test]
    fn test_rle_decode_empty() {
        // Just terminator '10'
        let data = vec![0b1000_0000];
        let mut reader = BitReader::new(&data, 2);

        let result = rle_decode(&mut reader, 8).unwrap();
        assert_eq!(result.len(), 8);
        assert_eq!(result.hamming_weight(), 0);
    }

    #[test]
    fn test_rle_decode_single_bit() {
        // COUNT(1) = '0', then '10' terminator
        // 010 = 0b010 -> 0b01000000 = 0x40
        let data = vec![0b0100_0000];
        let mut reader = BitReader::new(&data, 3);

        let result = rle_decode(&mut reader, 8).unwrap();
        assert_eq!(result.len(), 8);
        assert_eq!(result.hamming_weight(), 1);
        assert_eq!(result.get_bit(7), 1); // Last bit should be set
    }

    #[test]
    fn test_bit_insert() {
        // Insert bits 0, 1, 1 at mask positions 1, 4, 6
        let mask = BitVector::from_bytes(&[0b0100_1010], 8);
        let mut data = BitVector::new(8);
        data.zero();

        // Bits to insert: reversed order means we read 1, 1, 0
        // Data: 110 left-aligned = 0b11000000 = 0xC0
        let input = vec![0xC0];
        let mut reader = BitReader::new(&input, 3);

        bit_insert(&mut reader, &mut data, &mask).unwrap();

        // Check inserted bits
        assert_eq!(data.get_bit(1), 0); // First read goes to last position (6)
        assert_eq!(data.get_bit(4), 1); // Second read goes to middle (4)
        assert_eq!(data.get_bit(6), 1); // Third read goes to first (1)
    }

    #[test]
    fn test_bit_insert_length_mismatch() {
        let mask = BitVector::new(16);
        let mut data = BitVector::new(8);

        let input = vec![0x00];
        let mut reader = BitReader::new(&input, 8);

        assert!(bit_insert(&mut reader, &mut data, &mask).is_err());
    }

    #[test]
    fn test_bit_insert_empty_mask() {
        let mask = BitVector::new(8); // All zeros
        let mut data = BitVector::new(8);

        let input = vec![];
        let mut reader = BitReader::new(&input, 0);

        // Should succeed with no bits inserted
        bit_insert(&mut reader, &mut data, &mask).unwrap();
    }
}

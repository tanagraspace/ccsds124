//! Sequential bit reader for parsing compressed data.
//!
//! This module provides a bit reader for parsing compressed CCSDS 124.0-B-1 packets.
//! Bits are read MSB-first as per CCSDS 124.0-B-1.
//!
//! ## Bit Ordering
//! Bits are read MSB-first within each byte:
//! - Bit position 0 in a byte is bit 7 (MSB)
//! - Bit position 7 in a byte is bit 0 (LSB)

#![allow(clippy::cast_possible_truncation)]

use crate::error::Ccsds124Error;

/// Sequential bit reader for parsing compressed data.
///
/// Reads bits MSB-first from a byte slice.
#[derive(Clone, Debug)]
pub struct BitReader<'a> {
    /// Source data.
    data: &'a [u8],
    /// Total number of bits available.
    num_bits: usize,
    /// Current bit position.
    bit_pos: usize,
}

impl<'a> BitReader<'a> {
    /// Create a new bit reader.
    ///
    /// # Arguments
    /// * `data` - Source byte slice
    /// * `num_bits` - Total number of valid bits in the data
    pub fn new(data: &'a [u8], num_bits: usize) -> Self {
        Self {
            data,
            num_bits,
            bit_pos: 0,
        }
    }

    /// Get current bit position.
    #[inline]
    pub fn position(&self) -> usize {
        self.bit_pos
    }

    /// Get number of remaining bits.
    #[inline]
    pub fn remaining(&self) -> usize {
        self.num_bits.saturating_sub(self.bit_pos)
    }

    /// Check if there are more bits to read.
    #[inline]
    pub fn has_bits(&self) -> bool {
        self.bit_pos < self.num_bits
    }

    /// Read a single bit.
    ///
    /// # Returns
    /// The bit value (0 or 1), or error if no bits remain.
    #[inline]
    pub fn read_bit(&mut self) -> Result<u8, Ccsds124Error> {
        if self.bit_pos >= self.num_bits {
            return Err(Ccsds124Error::Underflow);
        }

        let byte_index = self.bit_pos >> 3; // / 8
        let bit_index = self.bit_pos & 7; // % 8

        // MSB-first: bit 0 of byte is at position 7
        let bit = (self.data[byte_index] >> (7 - bit_index)) & 1;

        self.bit_pos += 1;

        Ok(bit)
    }

    /// Read multiple bits into a u32.
    ///
    /// # Arguments
    /// * `num_bits` - Number of bits to read (1-32)
    ///
    /// # Returns
    /// The bits packed into a u32 (right-justified), or error.
    #[inline]
    pub fn read_bits(&mut self, num_bits: usize) -> Result<u32, Ccsds124Error> {
        if num_bits == 0 || num_bits > 32 {
            return Err(Ccsds124Error::InvalidLength);
        }

        if self.remaining() < num_bits {
            return Err(Ccsds124Error::Underflow);
        }

        // Optimized path: read bytes directly when possible
        let mut value = 0u32;
        let mut bits_remaining = num_bits;

        while bits_remaining > 0 {
            let byte_index = self.bit_pos >> 3;
            let bit_offset = self.bit_pos & 7;
            let bits_in_byte = 8 - bit_offset;
            let bits_to_read = bits_remaining.min(bits_in_byte);

            // Extract bits from current byte (MSB-first)
            let shift = bits_in_byte - bits_to_read;
            let mask = ((1u32 << bits_to_read) - 1) as u8;
            let bits = (self.data[byte_index] >> shift) & mask;

            value = (value << bits_to_read) | u32::from(bits);
            self.bit_pos += bits_to_read;
            bits_remaining -= bits_to_read;
        }

        Ok(value)
    }

    /// Align to next byte boundary.
    ///
    /// Skips remaining bits in the current byte if not already aligned.
    pub fn align_byte(&mut self) {
        let bit_offset = self.bit_pos % 8;
        if bit_offset != 0 {
            self.bit_pos += 8 - bit_offset;
        }
    }

    /// Peek at the next bit without consuming it.
    ///
    /// # Returns
    /// The bit value (0 or 1), or error if no bits remain.
    pub fn peek_bit(&self) -> Result<u8, Ccsds124Error> {
        if self.bit_pos >= self.num_bits {
            return Err(Ccsds124Error::Underflow);
        }

        let byte_index = self.bit_pos / 8;
        let bit_index = self.bit_pos % 8;

        let shifted = self.data[byte_index] >> (7 - bit_index);
        let bit = shifted & 1;

        Ok(bit)
    }

    /// Skip a number of bits.
    ///
    /// # Arguments
    /// * `count` - Number of bits to skip
    ///
    /// # Returns
    /// Ok(()) on success, or error if not enough bits remain.
    pub fn skip(&mut self, count: usize) -> Result<(), Ccsds124Error> {
        if self.remaining() < count {
            return Err(Ccsds124Error::Underflow);
        }

        self.bit_pos += count;
        Ok(())
    }

    /// Seek backwards by one bit position.
    ///
    /// Used in COUNT decoding when backtracking is needed.
    ///
    /// # Returns
    /// Ok(()) on success, or error if already at position 0.
    pub fn back(&mut self) -> Result<(), Ccsds124Error> {
        if self.bit_pos == 0 {
            return Err(Ccsds124Error::Underflow);
        }

        self.bit_pos -= 1;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new() {
        let data = vec![0xAB, 0xCD];
        let reader = BitReader::new(&data, 16);
        assert_eq!(reader.position(), 0);
        assert_eq!(reader.remaining(), 16);
        assert!(reader.has_bits());
    }

    #[test]
    fn test_read_bit() {
        // 0xAB = 10101011
        let data = vec![0xAB];
        let mut reader = BitReader::new(&data, 8);

        assert_eq!(reader.read_bit().unwrap(), 1); // bit 7
        assert_eq!(reader.read_bit().unwrap(), 0); // bit 6
        assert_eq!(reader.read_bit().unwrap(), 1); // bit 5
        assert_eq!(reader.read_bit().unwrap(), 0); // bit 4
        assert_eq!(reader.read_bit().unwrap(), 1); // bit 3
        assert_eq!(reader.read_bit().unwrap(), 0); // bit 2
        assert_eq!(reader.read_bit().unwrap(), 1); // bit 1
        assert_eq!(reader.read_bit().unwrap(), 1); // bit 0

        assert!(!reader.has_bits());
    }

    #[test]
    fn test_read_bits() {
        // 0xDE = 11011110, 0xAD = 10101101
        let data = vec![0xDE, 0xAD];
        let mut reader = BitReader::new(&data, 16);

        // Read 4 bits: 1101 = 13
        assert_eq!(reader.read_bits(4).unwrap(), 0b1101);

        // Read 8 bits: 11101010 = 234
        assert_eq!(reader.read_bits(8).unwrap(), 0b11101010);

        // Read 4 bits: 1101 = 13
        assert_eq!(reader.read_bits(4).unwrap(), 0b1101);

        assert_eq!(reader.remaining(), 0);
    }

    #[test]
    fn test_read_bits_underflow() {
        let data = vec![0xFF];
        let mut reader = BitReader::new(&data, 8);

        // Try to read more bits than available
        assert!(matches!(
            reader.read_bits(16),
            Err(Ccsds124Error::Underflow)
        ));
    }

    #[test]
    fn test_read_bits_invalid_count() {
        let data = vec![0xFF];
        let mut reader = BitReader::new(&data, 8);

        // Zero bits
        assert!(matches!(
            reader.read_bits(0),
            Err(Ccsds124Error::InvalidLength)
        ));

        // More than 32 bits
        assert!(matches!(
            reader.read_bits(33),
            Err(Ccsds124Error::InvalidLength)
        ));
    }

    #[test]
    fn test_align_byte() {
        let data = vec![0xAB, 0xCD];
        let mut reader = BitReader::new(&data, 16);

        // Read 3 bits
        reader.read_bits(3).unwrap();
        assert_eq!(reader.position(), 3);

        // Align to byte boundary
        reader.align_byte();
        assert_eq!(reader.position(), 8);

        // Already aligned, should stay at 8
        reader.align_byte();
        assert_eq!(reader.position(), 8);
    }

    #[test]
    fn test_peek_bit() {
        let data = vec![0xAB]; // 10101011
        let mut reader = BitReader::new(&data, 8);

        // Peek should not advance position
        assert_eq!(reader.peek_bit().unwrap(), 1);
        assert_eq!(reader.position(), 0);

        // Read should advance
        assert_eq!(reader.read_bit().unwrap(), 1);
        assert_eq!(reader.position(), 1);

        // Next peek
        assert_eq!(reader.peek_bit().unwrap(), 0);
        assert_eq!(reader.position(), 1);
    }

    #[test]
    fn test_skip() {
        let data = vec![0xAB, 0xCD];
        let mut reader = BitReader::new(&data, 16);

        reader.skip(4).unwrap();
        assert_eq!(reader.position(), 4);

        reader.skip(8).unwrap();
        assert_eq!(reader.position(), 12);

        // Skip remaining
        reader.skip(4).unwrap();
        assert_eq!(reader.remaining(), 0);

        // Try to skip more
        assert!(matches!(reader.skip(1), Err(Ccsds124Error::Underflow)));
    }

    #[test]
    fn test_back() {
        let data = vec![0xAB];
        let mut reader = BitReader::new(&data, 8);

        // Read a bit
        reader.read_bit().unwrap();
        assert_eq!(reader.position(), 1);

        // Go back
        reader.back().unwrap();
        assert_eq!(reader.position(), 0);

        // Try to go back at position 0
        assert!(matches!(reader.back(), Err(Ccsds124Error::Underflow)));
    }

    #[test]
    fn test_partial_bits() {
        // Test reading less than full byte
        let data = vec![0xF0]; // 11110000
        let mut reader = BitReader::new(&data, 5); // Only 5 bits valid

        assert_eq!(reader.read_bit().unwrap(), 1);
        assert_eq!(reader.read_bit().unwrap(), 1);
        assert_eq!(reader.read_bit().unwrap(), 1);
        assert_eq!(reader.read_bit().unwrap(), 1);
        assert_eq!(reader.read_bit().unwrap(), 0);

        // No more bits
        assert!(matches!(reader.read_bit(), Err(Ccsds124Error::Underflow)));
    }

    #[test]
    fn test_empty_reader() {
        let data = vec![];
        let mut reader = BitReader::new(&data, 0);

        assert!(!reader.has_bits());
        assert_eq!(reader.remaining(), 0);
        assert!(matches!(reader.read_bit(), Err(Ccsds124Error::Underflow)));
        assert!(matches!(reader.peek_bit(), Err(Ccsds124Error::Underflow)));
    }
}

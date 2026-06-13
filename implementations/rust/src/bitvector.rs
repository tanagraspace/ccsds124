//! Fixed-length bit vector implementation using 32-bit words.
//!
//! This module provides fixed-length bit vector operations optimized for
//! CCSDS 124.0-B-1 compression. Uses 32-bit words with big-endian byte packing to
//! match ESA/ESOC reference implementation.
//!
//! ## Bit Numbering Convention (CCSDS 124.0-B-1 Section 1.6.1)
//! - Bit 0 = MSB (Most Significant Bit, transmitted first)
//! - Bit N-1 = LSB (Least Significant Bit)
//!
//! ## Word Packing (Big-Endian)
//! Within each 32-bit word:
//! - Word\[i\] = (Byte\[4i\] << 24) | (Byte\[4i+1\] << 16) | (Byte\[4i+2\] << 8) | Byte\[4i+3\]
//! - Bit 0 = LSB of word, Bit 31 = MSB of word

#![allow(clippy::cast_possible_truncation)]
#![allow(clippy::cast_sign_loss)]
#![allow(clippy::return_self_not_must_use)]

/// Maximum packet length in bits (CCSDS max).
pub const MAX_PACKET_LENGTH: usize = 65535;

/// Fixed-length bit vector structure.
///
/// Stores a binary vector of length F bits using 32-bit words.
/// Bit 0 is the MSB (transmitted first), bit F-1 is the LSB.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct BitVector {
    /// 32-bit word storage (big-endian packing).
    data: Vec<u32>,
    /// Number of bits (F).
    length: usize,
}

impl BitVector {
    /// Create a new bit vector with specified length, initialized to zero.
    ///
    /// # Arguments
    /// * `num_bits` - Number of bits (1 to `MAX_PACKET_LENGTH`)
    ///
    /// # Returns
    /// A new `BitVector` with all bits set to zero.
    ///
    /// # Panics
    /// Panics if `num_bits` is 0 or exceeds `MAX_PACKET_LENGTH`.
    pub fn new(num_bits: usize) -> Self {
        assert!(num_bits > 0 && num_bits <= MAX_PACKET_LENGTH);

        // Calculate number of 32-bit words needed
        let num_bytes = (num_bits + 7) / 8;
        let num_words = (num_bytes + 3) / 4; // Ceiling division

        Self {
            data: vec![0u32; num_words],
            length: num_bits,
        }
    }

    /// Create a bit vector from raw bytes.
    ///
    /// # Arguments
    /// * `bytes` - Input byte slice
    /// * `num_bits` - Number of valid bits
    ///
    /// # Panics
    /// Panics if byte slice is too short for the specified bit count.
    pub fn from_bytes(bytes: &[u8], num_bits: usize) -> Self {
        assert!(num_bits > 0 && num_bits <= MAX_PACKET_LENGTH);

        let expected_bytes = (num_bits + 7) / 8;
        assert!(bytes.len() >= expected_bytes);

        let num_words = (expected_bytes + 3) / 4;
        let mut data = vec![0u32; num_words];

        // Pack bytes into 32-bit words (big-endian)
        let mut j = 4u32; // Counter for bytes within word (4, 3, 2, 1)
        let mut bytes_to_int = 0u32;
        let mut current_word = 0usize;

        for &byte in bytes.iter().take(expected_bytes) {
            j -= 1;
            bytes_to_int |= u32::from(byte) << (j * 8);

            if j == 0 {
                // Word complete - store it
                data[current_word] = bytes_to_int;
                current_word += 1;
                bytes_to_int = 0;
                j = 4;
            }
        }

        // Handle incomplete final word
        if j < 4 {
            data[current_word] = bytes_to_int;
        }

        Self {
            data,
            length: num_bits,
        }
    }

    /// Convert bit vector to bytes.
    ///
    /// # Returns
    /// A new `Vec<u8>` containing the bit vector data.
    pub fn to_bytes(&self) -> Vec<u8> {
        let expected_bytes = (self.length + 7) / 8;
        let mut result = Vec::with_capacity(expected_bytes);

        let mut byte_index = 0usize;
        for word in &self.data {
            // Extract up to 4 bytes from this word (big-endian)
            for j in (0u32..4).rev() {
                if byte_index >= expected_bytes {
                    break;
                }
                result.push((word >> (j * 8)) as u8);
                byte_index += 1;
            }
        }

        result
    }

    /// Set all bits to zero.
    pub fn zero(&mut self) {
        for word in &mut self.data {
            *word = 0;
        }
    }

    /// Get the length in bits.
    #[inline]
    pub fn len(&self) -> usize {
        self.length
    }

    /// Check if the bit vector is empty (zero length).
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.length == 0
    }

    /// Get raw access to the underlying 32-bit words.
    ///
    /// Used by encoding functions for efficient word-level operations.
    #[inline]
    pub fn words(&self) -> &[u32] {
        &self.data
    }

    /// Get bit value at position.
    ///
    /// # Arguments
    /// * `pos` - Bit position (0 = MSB, length-1 = LSB)
    ///
    /// # Returns
    /// Bit value (0 or 1), or 0 if position is out of bounds.
    #[inline]
    pub fn get_bit(&self, pos: usize) -> u8 {
        if pos >= self.length {
            return 0;
        }

        // Direct bit-to-word mapping (optimized):
        // word_index = pos / 32, bit_in_word = 31 - (pos % 32)
        // MSB-first: bit 0 is at position 31 in word 0
        let word_index = pos >> 5;
        let bit_in_word = 31 - (pos & 31);

        ((self.data[word_index] >> bit_in_word) & 1) as u8
    }

    /// Set bit value at position.
    ///
    /// # Arguments
    /// * `pos` - Bit position (0 = MSB, length-1 = LSB)
    /// * `value` - Bit value (0 or non-zero for 1)
    #[inline]
    pub fn set_bit(&mut self, pos: usize, value: u8) {
        if pos >= self.length {
            return;
        }

        // Direct bit-to-word mapping (optimized):
        // word_index = pos / 32, bit_in_word = 31 - (pos % 32)
        // MSB-first: bit 0 is at position 31 in word 0
        let word_index = pos >> 5;
        let bit_in_word = 31 - (pos & 31);

        if value != 0 {
            self.data[word_index] |= 1 << bit_in_word;
        } else {
            self.data[word_index] &= !(1 << bit_in_word);
        }
    }

    /// Bitwise XOR of two bit vectors.
    ///
    /// # Arguments
    /// * `other` - Other bit vector (must have same length)
    ///
    /// # Returns
    /// A new `BitVector` containing the XOR result.
    pub fn xor(&self, other: &Self) -> Self {
        let num_words = self.data.len().min(other.data.len());
        let mut result = Self::new(self.length);

        for i in 0..num_words {
            result.data[i] = self.data[i] ^ other.data[i];
        }

        result
    }

    /// Bitwise OR of two bit vectors.
    ///
    /// # Arguments
    /// * `other` - Other bit vector (must have same length)
    ///
    /// # Returns
    /// A new `BitVector` containing the OR result.
    pub fn or(&self, other: &Self) -> Self {
        let num_words = self.data.len().min(other.data.len());
        let mut result = Self::new(self.length);

        for i in 0..num_words {
            result.data[i] = self.data[i] | other.data[i];
        }

        result
    }

    /// In-place bitwise OR with another bit vector.
    ///
    /// # Arguments
    /// * `other` - Other bit vector (must have same length)
    #[inline]
    pub fn or_assign(&mut self, other: &Self) {
        let num_words = self.data.len().min(other.data.len());
        for i in 0..num_words {
            self.data[i] |= other.data[i];
        }
    }

    /// Bitwise AND of two bit vectors.
    ///
    /// # Arguments
    /// * `other` - Other bit vector (must have same length)
    ///
    /// # Returns
    /// A new `BitVector` containing the AND result.
    pub fn and(&self, other: &Self) -> Self {
        let num_words = self.data.len().min(other.data.len());
        let mut result = Self::new(self.length);

        for i in 0..num_words {
            result.data[i] = self.data[i] & other.data[i];
        }

        result
    }

    /// Bitwise NOT of the bit vector.
    ///
    /// # Returns
    /// A new `BitVector` containing the NOT result.
    pub fn not(&self) -> Self {
        let mut result = Self::new(self.length);

        for i in 0..self.data.len() {
            result.data[i] = !self.data[i];
        }

        // Mask off unused bits in last word with big-endian packing
        if !self.data.is_empty() {
            let num_bytes = (self.length + 7) / 8;
            let bytes_in_last_word = ((num_bytes - 1) % 4) + 1;
            let bits_in_last_byte = self.length - ((num_bytes - 1) * 8);

            // Create mask for valid bits in big-endian word.
            // MSB-first: valid bits in a partial last byte are at the HIGH
            // end of the byte, so mask 0xFF << (8 - bits) (GOTCHAS #19).
            let mut mask = 0u32;
            for byte in 0..bytes_in_last_word {
                let byte_mask: u8 = if byte == bytes_in_last_word - 1 {
                    // Handle case where bits_in_last_byte is 8 (full byte)
                    if bits_in_last_byte >= 8 {
                        0xFF
                    } else {
                        (0xFFu32 << (8 - bits_in_last_byte)) as u8
                    }
                } else {
                    0xFF
                };
                let shift_amt = (3 - byte as u32) * 8;
                mask |= u32::from(byte_mask) << shift_amt;
            }
            result.data[self.data.len() - 1] &= mask;
        }

        result
    }

    /// Left shift the bit vector by 1 position.
    ///
    /// MSB-first with big-endian word packing:
    /// Left shift means shift towards MSB (bit 0).
    ///
    /// # Returns
    /// A new `BitVector` containing the shifted result.
    pub fn left_shift(&self) -> Self {
        let mut result = Self::new(self.length);

        if !self.data.is_empty() {
            // Process words from first (MSB) to last (LSB)
            for i in 0..self.data.len() - 1 {
                // Shift current word left by 1, bring in MSB from next word
                result.data[i] = (self.data[i] << 1) | (self.data[i + 1] >> 31);
            }
            // Last word: shift left, LSB becomes 0
            result.data[self.data.len() - 1] = self.data[self.data.len() - 1] << 1;
        }

        result
    }

    /// Calculate the Hamming weight (number of 1 bits).
    ///
    /// # Returns
    /// Count of bits set to 1.
    pub fn hamming_weight(&self) -> usize {
        let mut count = 0usize;

        // Count '1' bits in each word using popcount
        for &word in &self.data {
            count += word.count_ones() as usize;
        }

        // Adjust for any extra bits in last word
        let num_bytes = (self.length + 7) / 8;
        let extra_bits = (num_bytes * 8) - self.length;
        if extra_bits > 0 && !self.data.is_empty() {
            // Count bits in the unused portion of the last word and subtract
            let last_word = self.data[self.data.len() - 1];
            let mask = (1u32 << extra_bits) - 1; // Mask for the unused LSBs
            let extra_word = last_word & mask;
            count -= extra_word.count_ones() as usize;
        }

        count
    }

    /// Reverse the bit order.
    ///
    /// # Returns
    /// A new `BitVector` with reversed bit order.
    pub fn reverse(&self) -> Self {
        let mut result = Self::new(self.length);

        for i in 0..self.length {
            let bit = self.get_bit(i);
            result.set_bit(self.length - 1 - i, bit);
        }

        result
    }

    /// Copy from another bit vector.
    ///
    /// # Arguments
    /// * `other` - Source bit vector
    #[inline]
    pub fn copy_from(&mut self, other: &Self) {
        if self.data.len() == other.data.len() {
            // Fast path: same size, direct copy
            self.data.copy_from_slice(&other.data);
        } else {
            // Slow path: resize needed
            self.data.resize(other.data.len(), 0);
            self.data.copy_from_slice(&other.data);
        }
        self.length = other.length;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new() {
        let bv = BitVector::new(720);
        assert_eq!(bv.len(), 720);
        assert_eq!(bv.hamming_weight(), 0);
    }

    #[test]
    fn test_get_set_bit() {
        let mut bv = BitVector::new(32);

        // Set bit 0 (LSB)
        bv.set_bit(0, 1);
        assert_eq!(bv.get_bit(0), 1);
        assert_eq!(bv.get_bit(1), 0);

        // Set bit 31 (MSB for 32-bit)
        bv.set_bit(31, 1);
        assert_eq!(bv.get_bit(31), 1);

        // Clear bit 0
        bv.set_bit(0, 0);
        assert_eq!(bv.get_bit(0), 0);
    }

    #[test]
    fn test_from_bytes_to_bytes_roundtrip() {
        let original = vec![0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE];
        let bv = BitVector::from_bytes(&original, 48);
        let result = bv.to_bytes();
        assert_eq!(result, original);
    }

    #[test]
    fn test_xor() {
        let mut a = BitVector::new(32);
        let mut b = BitVector::new(32);

        a.set_bit(0, 1);
        a.set_bit(1, 1);
        b.set_bit(1, 1);
        b.set_bit(2, 1);

        let result = a.xor(&b);
        assert_eq!(result.get_bit(0), 1); // 1 XOR 0 = 1
        assert_eq!(result.get_bit(1), 0); // 1 XOR 1 = 0
        assert_eq!(result.get_bit(2), 1); // 0 XOR 1 = 1
    }

    #[test]
    fn test_or() {
        let mut a = BitVector::new(32);
        let mut b = BitVector::new(32);

        a.set_bit(0, 1);
        b.set_bit(1, 1);

        let result = a.or(&b);
        assert_eq!(result.get_bit(0), 1);
        assert_eq!(result.get_bit(1), 1);
        assert_eq!(result.get_bit(2), 0);
    }

    #[test]
    fn test_and() {
        let mut a = BitVector::new(32);
        let mut b = BitVector::new(32);

        a.set_bit(0, 1);
        a.set_bit(1, 1);
        b.set_bit(1, 1);
        b.set_bit(2, 1);

        let result = a.and(&b);
        assert_eq!(result.get_bit(0), 0); // 1 AND 0 = 0
        assert_eq!(result.get_bit(1), 1); // 1 AND 1 = 1
        assert_eq!(result.get_bit(2), 0); // 0 AND 1 = 0
    }

    #[test]
    fn test_not() {
        let mut bv = BitVector::new(8);
        bv.set_bit(0, 1);
        bv.set_bit(2, 1);

        let result = bv.not();
        assert_eq!(result.get_bit(0), 0);
        assert_eq!(result.get_bit(1), 1);
        assert_eq!(result.get_bit(2), 0);
        assert_eq!(result.get_bit(3), 1);
    }

    // GOTCHAS #19 (issue #103): NOT of a non-byte-aligned vector must set the
    // valid (high) bits and leave padding zero. NOT of an all-zero length-12
    // vector: positions 0..12 must all be 1.
    #[test]
    fn test_not_msb_aligned_non_byte_aligned() {
        let bv = BitVector::new(12);
        let result = bv.not();
        for i in 0..12 {
            assert_eq!(result.get_bit(i), 1, "position {i} should be set");
        }
        assert_eq!(result.hamming_weight(), 12);
    }

    #[test]
    fn test_left_shift() {
        let mut bv = BitVector::new(32);
        bv.set_bit(1, 1); // Set bit 1

        let result = bv.left_shift();
        assert_eq!(result.get_bit(0), 1); // Bit 1 shifted to bit 0
        assert_eq!(result.get_bit(1), 0);
    }

    #[test]
    fn test_hamming_weight() {
        let mut bv = BitVector::new(32);
        assert_eq!(bv.hamming_weight(), 0);

        bv.set_bit(0, 1);
        bv.set_bit(5, 1);
        bv.set_bit(31, 1);
        assert_eq!(bv.hamming_weight(), 3);
    }

    #[test]
    fn test_zero() {
        let mut bv = BitVector::new(32);
        bv.set_bit(0, 1);
        bv.set_bit(15, 1);
        bv.set_bit(31, 1);

        bv.zero();
        assert_eq!(bv.hamming_weight(), 0);
    }

    #[test]
    fn test_equals() {
        let mut a = BitVector::new(32);
        let mut b = BitVector::new(32);

        assert_eq!(a, b);

        a.set_bit(5, 1);
        assert_ne!(a, b);

        b.set_bit(5, 1);
        assert_eq!(a, b);
    }

    #[test]
    fn test_720_bits() {
        // Test with CCSDS 124.0-B-1 standard packet size
        let mut bv = BitVector::new(720);
        assert_eq!(bv.len(), 720);

        // Set first and last bits
        bv.set_bit(0, 1);
        bv.set_bit(719, 1);
        assert_eq!(bv.get_bit(0), 1);
        assert_eq!(bv.get_bit(719), 1);
        assert_eq!(bv.hamming_weight(), 2);

        // Round-trip through bytes
        let bytes = bv.to_bytes();
        assert_eq!(bytes.len(), 90); // 720 / 8 = 90 bytes

        let bv2 = BitVector::from_bytes(&bytes, 720);
        assert_eq!(bv, bv2);
    }

    #[test]
    fn test_reverse() {
        let mut bv = BitVector::new(8);
        bv.set_bit(0, 1); // bit 0
        bv.set_bit(1, 0);
        bv.set_bit(2, 1); // bit 2

        let rev = bv.reverse();
        assert_eq!(rev.get_bit(7), 1); // bit 0 -> bit 7
        assert_eq!(rev.get_bit(6), 0);
        assert_eq!(rev.get_bit(5), 1); // bit 2 -> bit 5
    }

    #[test]
    fn test_default() {
        let bv = BitVector::default();
        assert_eq!(bv.len(), 0);
        assert!(bv.is_empty());
    }
}

package ccsds124

import (
	"errors"
	"math/bits"
)

// BitVector is a fixed-length bit vector using 32-bit word storage.
//
// Bit Numbering Convention (CCSDS 124.0-B-1 Section 1.6.1):
//   - Bit 0 = MSB (Most Significant Bit, transmitted first)
//   - Bit N-1 = LSB (Least Significant Bit)
//
// Word Packing (Big-Endian):
//   - Word[i] contains Bytes [4i, 4i+1, 4i+2, 4i+3]
//   - Byte 4i at bits 24-31 (most significant)
//   - Byte 4i+3 at bits 0-7 (least significant)
type BitVector struct {
	data     []uint32
	length   int // Length in bits
	numWords int
}

// NewBitVector creates a new bit vector with the specified length in bits.
func NewBitVector(numBits int) (*BitVector, error) {
	if numBits <= 0 {
		return nil, errors.New("numBits must be positive")
	}

	// Calculate number of 32-bit words needed
	numBytes := (numBits + 7) / 8
	numWords := (numBytes + 3) / 4

	return &BitVector{
		data:     make([]uint32, numWords),
		length:   numBits,
		numWords: numWords,
	}, nil
}

// Length returns the length of the bit vector in bits.
func (bv *BitVector) Length() int {
	return bv.length
}

// Zero sets all bits to zero.
func (bv *BitVector) Zero() {
	for i := range bv.data {
		bv.data[i] = 0
	}
}

// Copy creates a copy of this bit vector.
func (bv *BitVector) Copy() *BitVector {
	result := &BitVector{
		data:     make([]uint32, bv.numWords),
		length:   bv.length,
		numWords: bv.numWords,
	}
	copy(result.data, bv.data)
	return result
}

// CopyFrom copies contents from another bit vector into this one.
func (bv *BitVector) CopyFrom(other *BitVector) {
	numWords := bv.numWords
	if other.numWords < numWords {
		numWords = other.numWords
	}
	for i := 0; i < numWords; i++ {
		bv.data[i] = other.data[i]
	}
}

// GetBit returns the bit value at the specified position.
// Bit 0 is the MSB, bit length-1 is the LSB.
func (bv *BitVector) GetBit(pos int) int {
	if pos < 0 || pos >= bv.length {
		return 0
	}

	// Optimized: single division, bit operations for the rest
	// pos/8 = byte index, pos&7 = bit within byte
	// byte/4 = word index, byte&3 = byte within word
	wordIndex := pos >> 5 // pos / 32
	bitInWord := 31 - (pos & 31)

	return int((bv.data[wordIndex] >> bitInWord) & 1)
}

// SetBit sets the bit value at the specified position.
// Bit 0 is the MSB, bit length-1 is the LSB.
func (bv *BitVector) SetBit(pos int, value int) {
	if pos < 0 || pos >= bv.length {
		return
	}

	// Optimized: use bit operations instead of multiple divisions
	wordIndex := pos >> 5 // pos / 32
	bitInWord := 31 - (pos & 31)

	if value != 0 {
		// Set bit to 1
		bv.data[wordIndex] |= 1 << bitInWord
	} else {
		// Clear bit to 0
		bv.data[wordIndex] &= ^(1 << bitInWord)
	}
}

// FromBytes loads the bit vector from bytes (big-endian).
func (bv *BitVector) FromBytes(data []byte) {
	// Zero the array first
	bv.Zero()

	// Pack bytes into 32-bit words (big-endian)
	j := 4 // Counter for bytes within word (4, 3, 2, 1)
	var bytesToInt uint32
	currentWord := 0

	for _, b := range data {
		j--
		bytesToInt |= uint32(b) << (j * 8)

		if j == 0 {
			// Word complete - store it
			bv.data[currentWord] = bytesToInt
			currentWord++
			bytesToInt = 0
			j = 4
		}
	}

	// Handle incomplete final word
	if j < 4 {
		bv.data[currentWord] = bytesToInt
	}
}

// ToBytes converts the bit vector to bytes (big-endian).
func (bv *BitVector) ToBytes() []byte {
	expectedBytes := (bv.length + 7) / 8
	result := make([]byte, expectedBytes)

	byteIndex := 0
	for wordIndex := 0; wordIndex < bv.numWords && byteIndex < expectedBytes; wordIndex++ {
		word := bv.data[wordIndex]

		// Extract up to 4 bytes from this word
		for j := 3; j >= 0 && byteIndex < expectedBytes; j-- {
			result[byteIndex] = byte((word >> (j * 8)) & 0xFF)
			byteIndex++
		}
	}

	return result
}

// XOR computes the bitwise XOR of this vector with another.
func (bv *BitVector) XOR(other *BitVector) *BitVector {
	result, _ := NewBitVector(bv.length)

	numWords := bv.numWords
	if other.numWords < numWords {
		numWords = other.numWords
	}

	for i := 0; i < numWords; i++ {
		result.data[i] = bv.data[i] ^ other.data[i]
	}

	return result
}

// XORInto computes a XOR b and stores the result in this vector.
func (bv *BitVector) XORInto(a, b *BitVector) {
	numWords := bv.numWords
	if a.numWords < numWords {
		numWords = a.numWords
	}
	if b.numWords < numWords {
		numWords = b.numWords
	}

	for i := 0; i < numWords; i++ {
		bv.data[i] = a.data[i] ^ b.data[i]
	}
}

// OR computes the bitwise OR of this vector with another.
func (bv *BitVector) OR(other *BitVector) *BitVector {
	result, _ := NewBitVector(bv.length)

	numWords := bv.numWords
	if other.numWords < numWords {
		numWords = other.numWords
	}

	for i := 0; i < numWords; i++ {
		result.data[i] = bv.data[i] | other.data[i]
	}

	return result
}

// ORInto computes a OR b and stores the result in this vector.
func (bv *BitVector) ORInto(a, b *BitVector) {
	numWords := bv.numWords
	if a.numWords < numWords {
		numWords = a.numWords
	}
	if b.numWords < numWords {
		numWords = b.numWords
	}

	for i := 0; i < numWords; i++ {
		bv.data[i] = a.data[i] | b.data[i]
	}
}

// AND computes the bitwise AND of this vector with another.
func (bv *BitVector) AND(other *BitVector) *BitVector {
	result, _ := NewBitVector(bv.length)

	numWords := bv.numWords
	if other.numWords < numWords {
		numWords = other.numWords
	}

	for i := 0; i < numWords; i++ {
		result.data[i] = bv.data[i] & other.data[i]
	}

	return result
}

// NOT computes the bitwise NOT (inversion) of this vector.
func (bv *BitVector) NOT() *BitVector {
	result, _ := NewBitVector(bv.length)

	for i := 0; i < bv.numWords; i++ {
		result.data[i] = ^bv.data[i]
	}

	// Mask off unused bits in last word
	if bv.numWords > 0 {
		numBytes := (bv.length + 7) / 8
		bytesInLastWord := ((numBytes - 1) % 4) + 1
		bitsInLastByte := bv.length - ((numBytes - 1) * 8)

		// Create mask for valid bits in big-endian word
		// For MSB-first indexing, valid bits in partial byte are at the high end
		var mask uint32
		for b := 0; b < bytesInLastWord; b++ {
			var byteMask uint32
			if b == bytesInLastWord-1 {
				// Partial byte: create mask for high bits (MSB-first)
				byteMask = uint32(0xFF) << (8 - bitsInLastByte) & 0xFF
			} else {
				byteMask = 0xFF
			}
			shiftAmt := (3 - b) * 8
			mask |= byteMask << shiftAmt
		}
		result.data[bv.numWords-1] &= mask
	}

	return result
}

// LeftShift computes a left shift by 1 position.
// Left shift moves bits toward MSB (lower indices).
// MSB is lost, LSB becomes 0.
func (bv *BitVector) LeftShift() *BitVector {
	result, _ := NewBitVector(bv.length)

	// Word-level left shift (big-endian: MSB in high bits of first word)
	// Shift each word left by 1, carry MSB from next word
	var carry uint32
	for i := bv.numWords - 1; i >= 0; i-- {
		word := bv.data[i]
		result.data[i] = ((word << 1) | carry) & 0xFFFFFFFF
		carry = (word >> 31) & 1
	}

	return result
}

// Reverse returns a new bit vector with reversed bit order.
func (bv *BitVector) Reverse() *BitVector {
	result, _ := NewBitVector(bv.length)

	for i := 0; i < bv.length; i++ {
		bit := bv.GetBit(i)
		result.SetBit((bv.length-1)-i, bit)
	}

	return result
}

// HammingWeight returns the number of 1 bits.
func (bv *BitVector) HammingWeight() int {
	count := 0

	// Count '1' bits in each word using hardware POPCNT
	for i := 0; i < bv.numWords; i++ {
		count += bits.OnesCount32(bv.data[i])
	}

	// Adjust for any extra bits in last word
	numBytes := (bv.length + 7) / 8
	extraBits := (numBytes * 8) - bv.length
	if extraBits > 0 && bv.numWords > 0 {
		// Count bits in the unused portion of the last word and subtract
		lastWord := bv.data[bv.numWords-1]
		mask := uint32((1 << extraBits) - 1)
		count -= bits.OnesCount32(lastWord & mask)
	}

	return count
}

// Equals checks if this bit vector equals another.
func (bv *BitVector) Equals(other *BitVector) bool {
	if bv.length != other.length {
		return false
	}

	for i := 0; i < bv.numWords; i++ {
		if bv.data[i] != other.data[i] {
			return false
		}
	}

	return true
}

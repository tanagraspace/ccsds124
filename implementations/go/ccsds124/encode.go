package ccsds124

import (
	"errors"
	"math/bits"
)

// CountEncode implements CCSDS 124.0-B-1 Section 5.2.2, Table 5-1, Equation 9.
//
// Encodes positive integers 1 <= A <= 2^16 - 1:
//   - A = 1 -> '0'
//   - 2 <= A <= 33 -> '110' || BIT_5(A-2)
//   - A >= 34 -> '111' || BIT_E(A-2) where E = 2*floor(log2(A-2)+1) - 6
func CountEncode(bb *BitBuffer, A int) error {
	if A < 1 || A > 65535 {
		return errors.New("COUNT: A must be in range [1, 65535]")
	}

	if A == 1 {
		// Case 1: A = 1 -> '0'
		bb.AppendBit(0)
	} else if A <= 33 {
		// Case 2: 2 <= A <= 33 -> '110' || BIT_5(A-2)
		bb.AppendBit(1)
		bb.AppendBit(1)
		bb.AppendBit(0)

		// Append BIT_5(A-2) MSB-first - 5 bits
		value := A - 2
		for i := 4; i >= 0; i-- {
			bit := (value >> i) & 1
			bb.AppendBit(bit)
		}
	} else {
		// Case 3: A >= 34 -> '111' || BIT_E(A-2)
		bb.AppendBit(1)
		bb.AppendBit(1)
		bb.AppendBit(1)

		// Calculate E = 2*floor(log2(A-2)+1) - 6
		// Using bits.Len for fast integer log2: floor(log2(x)) = bits.Len(x) - 1
		value := A - 2
		E := (2 * bits.Len(uint(value))) - 6

		// Append BIT_E(A-2) MSB-first - E bits
		for i := E - 1; i >= 0; i-- {
			bit := (value >> i) & 1
			bb.AppendBit(bit)
		}
	}

	return nil
}

// CountEncodeTerminator writes the RLE terminator pattern '10'.
func CountEncodeTerminator(bb *BitBuffer) {
	bb.AppendBit(1)
	bb.AppendBit(0)
}

// DeBruijn lookup table for fast LSB finding (matches reference implementation)
var debruijnLookup = [32]int{
	1, 2, 29, 3, 30, 15, 25, 4, 31, 23, 21, 16,
	26, 18, 5, 9, 32, 28, 14, 24, 22, 20, 17, 8,
	27, 13, 19, 7, 12, 6, 11, 10,
}

// RLEEncode implements CCSDS 124.0-B-1 Section 5.2.3, Equation 10.
//
// RLE(a) = COUNT(C_0) || COUNT(C_1) || ... || COUNT(C_{H(a)-1}) || '10'
//
// where C_i = 1 + (count of consecutive '0' bits before i-th '1' bit)
// and H(a) = Hamming weight (number of '1' bits in a)
//
// Note: Trailing zeros are not encoded (deducible from vector length)
func RLEEncode(bb *BitBuffer, input *BitVector) error {
	if input == nil {
		return errors.New("RLE: input cannot be nil")
	}

	// Start from the end of the vector
	oldBitPosition := input.length

	// Process words in reverse order (from high to low)
	for word := input.numWords - 1; word >= 0; word-- {
		wordData := input.data[word]

		// Process all set bits in this word
		for wordData != 0 {
			// Isolate the LSB: x = change & -change
			lsb := wordData & uint32(-int32(wordData))

			// Find LSB position using DeBruijn sequence
			debruijnIndex := (lsb * 0x077CB531) >> 27
			bitPositionInWord := debruijnLookup[debruijnIndex]

			// Count from the other side (reference line 754)
			bitPositionInWord = 32 - bitPositionInWord

			// Calculate global bit position (reference line 756)
			newBitPosition := (word * 32) + bitPositionInWord

			// Calculate delta (number of zeros + 1)
			delta := oldBitPosition - newBitPosition

			// Encode the count
			if err := CountEncode(bb, delta); err != nil {
				return err
			}

			// Update old position for next iteration
			oldBitPosition = newBitPosition

			// Clear the processed bit
			wordData ^= lsb
		}
	}

	// Append terminator '10'
	CountEncodeTerminator(bb)

	return nil
}

// BitExtract implements CCSDS 124.0-B-1 Section 5.2.4, Equation 11.
//
// BE(a, b) = a_{g_{H(b)-1}} || ... || a_{g_1} || a_{g_0}
//
// where g_i is the position of the i-th '1' bit in b (MSB to LSB order)
//
// Extracts bits from 'data' at positions where 'mask' has '1' bits.
// Output order: highest position to lowest position
func BitExtract(bb *BitBuffer, data, mask *BitVector) error {
	if data == nil || mask == nil {
		return errors.New("BitExtract: data and mask cannot be nil")
	}
	if data.length != mask.length {
		return errors.New("BitExtract: data and mask must have same length")
	}

	// Process words from last to first (high positions to low)
	// Within each word: bit 0 = highest position, bit 31 = lowest position
	for w := mask.numWords - 1; w >= 0; w-- {
		maskWord := mask.data[w]
		dataWord := data.data[w]

		if maskWord == 0 {
			continue
		}

		// Extract bits from low bit index to high (= high position to low position)
		// Use isolate-LSB technique to process from bit 0 upward
		for maskWord != 0 {
			// Isolate lowest set bit: lsb = x & -x
			lsb := maskWord & uint32(-int32(maskWord))

			// Get bit position using trailing zeros
			bitPos := bits.TrailingZeros32(lsb)

			// Extract data bit at that position
			dataBit := int((dataWord >> bitPos) & 1)
			bb.AppendBit(dataBit)

			// Clear the processed bit
			maskWord ^= lsb
		}
	}

	return nil
}

// BitExtractForward extracts bits in forward order (lowest position to highest).
// Used for kt component: processes mask values at changed positions
// in order from lowest position index to highest.
func BitExtractForward(bb *BitBuffer, data, mask *BitVector) error {
	if data == nil || mask == nil {
		return errors.New("BitExtractForward: data and mask cannot be nil")
	}
	if data.length != mask.length {
		return errors.New("BitExtractForward: data and mask must have same length")
	}

	// Process words from first to last (low positions to high)
	for w := 0; w < mask.numWords; w++ {
		maskWord := mask.data[w]
		dataWord := data.data[w]

		if maskWord == 0 {
			continue
		}

		// Extract bits from high bit index to low (= low position to high position)
		for maskWord != 0 {
			// Find highest set bit (corresponds to lowest position in this word's range)
			highBit := 31 - bits.LeadingZeros32(maskWord)

			dataBit := int((dataWord >> highBit) & 1)
			bb.AppendBit(dataBit)

			maskWord &= ^(uint32(1) << highBit)
		}
	}

	return nil
}

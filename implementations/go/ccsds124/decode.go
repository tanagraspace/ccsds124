package ccsds124

import (
	"fmt"
	"math/bits"
)

// CountDecode decodes a COUNT-encoded value.
//
// Decoding rules (inverse of CCSDS Equation 9):
//   - '0' -> 1
//   - '10' -> 0 (terminator)
//   - '110' + BIT5 -> value + 2 (range 2-33)
//   - '111' + BIT_E -> value + 2 (range 34+)
//
// Returns decoded positive integer (0 for terminator, 1+ for values).
func CountDecode(br *BitReader) (int, error) {
	// Read first bit
	firstBit, err := br.ReadBit()
	if err != nil {
		return 0, fmt.Errorf("COUNT decode: %w", err)
	}

	if firstBit == 0 {
		// Case 1: '0' -> 1
		return 1, nil
	}

	// First bit was 1, read second bit
	secondBit, err := br.ReadBit()
	if err != nil {
		return 0, fmt.Errorf("COUNT decode: %w", err)
	}

	if secondBit == 0 {
		// Case 2: '10' -> terminator (0)
		return 0, nil
	}

	// First two bits were '11', read third bit
	thirdBit, err := br.ReadBit()
	if err != nil {
		return 0, fmt.Errorf("COUNT decode: %w", err)
	}

	if thirdBit == 0 {
		// Case 3: '110' + BIT5 -> value + 2 (range 2-33)
		value, err := br.ReadBits(5)
		if err != nil {
			return 0, fmt.Errorf("COUNT decode BIT5: %w", err)
		}
		return int(value) + 2, nil
	}

	// Case 4: '111' + BIT_E -> value + 2 (range 34+)
	// Need to determine E by reading bits until we can decode
	// E = 2*floor(log2(value)+1) - 6
	// We read bits incrementally to find the value

	// Start with 6 bits (minimum for value >= 32)
	e := 6
	value, err := br.ReadBits(e)
	if err != nil {
		return 0, fmt.Errorf("COUNT decode BIT_E: %w", err)
	}

	// Check if we have the right number of bits
	// For value v, E should be 2*floor(log2(v)+1) - 6
	for {
		// Calculate expected E for this value
		var expectedE int
		if value == 0 {
			expectedE = 0
		} else {
			// Find floor(log2(value))
			logVal := 0
			temp := value
			for temp > 1 {
				temp >>= 1
				logVal++
			}
			expectedE = 2*(logVal+1) - 6
		}

		if expectedE == e {
			// We have the right number of bits
			break
		}

		// Need more bits
		e += 2
		// Read 2 more bits and shift in
		extra, err := br.ReadBits(2)
		if err != nil {
			return 0, fmt.Errorf("COUNT decode extra bits: %w", err)
		}
		value = (value << 2) | extra
	}

	return int(value) + 2, nil
}

// RLEDecode decodes an RLE-encoded bit vector.
//
// Decoding rules (inverse of CCSDS Equation 10):
//   - Read COUNT values until terminator (0)
//   - Each COUNT value represents position delta to next '1' bit
func RLEDecode(br *BitReader, length int) (*BitVector, error) {
	result, err := NewBitVector(length)
	if err != nil {
		return nil, fmt.Errorf("RLE decode: %w", err)
	}

	// Start from end of vector
	position := length

	for {
		// Decode next count
		count, err := CountDecode(br)
		if err != nil {
			return nil, fmt.Errorf("RLE decode: %w", err)
		}

		if count == 0 {
			// Terminator reached
			break
		}

		// Move position back by count. A delta beyond the remaining bit
		// position means the encoding is invalid for this vector length
		// (GOTCHAS #20): reject it instead of silently skipping.
		if count > position {
			return nil, fmt.Errorf("RLE decode: invalid delta %d exceeds remaining bit position %d", count, position)
		}
		position -= count

		// Set the bit at this position
		result.SetBit(position, 1)
	}

	return result, nil
}

// BitInsert inserts bits into data at positions specified by mask (inverse of BE).
//
// Reads bits from reader and inserts them into data at positions
// where mask has '1' bits. Bits are read in reverse order (highest
// position to lowest) to match BE extraction order.
func BitInsert(br *BitReader, data, mask *BitVector) error {
	if data == nil || mask == nil {
		return fmt.Errorf("BitInsert: data and mask cannot be nil")
	}
	if data.length != mask.length {
		return fmt.Errorf("BitInsert: data and mask must have same length")
	}

	// Process words from last to first (high positions to low, matching BitExtract)
	for w := mask.numWords - 1; w >= 0; w-- {
		maskWord := mask.data[w]

		if maskWord == 0 {
			continue
		}

		// Process from low bit index to high (= high position to low)
		for maskWord != 0 {
			// Isolate lowest set bit
			lsb := maskWord & uint32(-int32(maskWord))
			bitPos := bits.TrailingZeros32(lsb)

			bit, err := br.ReadBit()
			if err != nil {
				return fmt.Errorf("BitInsert: %w", err)
			}

			if bit != 0 {
				data.data[w] |= uint32(1) << bitPos
			} else {
				data.data[w] &= ^(uint32(1) << bitPos)
			}

			maskWord ^= lsb
		}
	}

	return nil
}

// BitInsertForward inserts bits in forward order (for kt component).
//
// Same as BitInsert but reads bits from lowest position to highest.
func BitInsertForward(br *BitReader, data, mask *BitVector) error {
	if data == nil || mask == nil {
		return fmt.Errorf("BitInsertForward: data and mask cannot be nil")
	}
	if data.length != mask.length {
		return fmt.Errorf("BitInsertForward: data and mask must have same length")
	}

	// Process words from first to last (low positions to high)
	for w := 0; w < mask.numWords; w++ {
		maskWord := mask.data[w]

		if maskWord == 0 {
			continue
		}

		// Process from high bit index to low (= low position to high)
		for maskWord != 0 {
			highBit := 31 - bits.LeadingZeros32(maskWord)

			bit, err := br.ReadBit()
			if err != nil {
				return fmt.Errorf("BitInsertForward: %w", err)
			}

			if bit != 0 {
				data.data[w] |= uint32(1) << highBit
			} else {
				data.data[w] &= ^(uint32(1) << highBit)
			}

			maskWord &= ^(uint32(1) << highBit)
		}
	}

	return nil
}

package ccsds124

// BitBuffer is a variable-length bit buffer for building compressed output.
//
// Bits are appended sequentially using MSB-first ordering as required by
// CCSDS 124.0-B-1.
//
// Bit Ordering:
//   - First bit appended goes to bit position 7
//   - Second bit goes to position 6, etc.
type BitBuffer struct {
	data    []byte
	numBits int
	// Accumulator for batch bit operations
	acc    uint64 // accumulate up to 64 bits
	accLen int    // number of bits in accumulator
}

// NewBitBuffer creates a new empty bit buffer.
func NewBitBuffer() *BitBuffer {
	return &BitBuffer{
		data:    make([]byte, 0, 256), // Pre-allocate reasonable capacity
		numBits: 0,
	}
}

// Clear resets the buffer to empty.
func (bb *BitBuffer) Clear() {
	bb.data = bb.data[:0]
	bb.numBits = 0
	bb.acc = 0
	bb.accLen = 0
}

// NumBits returns the number of bits in the buffer.
func (bb *BitBuffer) NumBits() int {
	return bb.numBits
}

// flushAcc writes accumulated bits to the data slice.
func (bb *BitBuffer) flushAcc() {
	for bb.accLen >= 8 {
		// Extract top 8 bits
		bb.accLen -= 8
		b := byte(bb.acc >> bb.accLen)
		bb.data = append(bb.data, b)
		bb.acc &= (1 << bb.accLen) - 1 // clear extracted bits
	}
}

// AppendBit appends a single bit to the buffer.
func (bb *BitBuffer) AppendBit(bit int) {
	bb.acc = (bb.acc << 1) | uint64(bit&1)
	bb.accLen++
	bb.numBits++

	// Flush when accumulator has 8+ bits
	if bb.accLen >= 8 {
		bb.flushAcc()
	}
}

// AppendBitsFromWord appends n bits from a 32-bit word (MSB-first).
// The bits are taken from the top n bits of the word.
func (bb *BitBuffer) AppendBitsFromWord(word uint32, n int) {
	if n <= 0 {
		return
	}
	// Shift word so the n bits are at the top
	bits := uint64(word >> (32 - n))
	bb.acc = (bb.acc << n) | bits
	bb.accLen += n
	bb.numBits += n

	// Flush complete bytes
	if bb.accLen >= 8 {
		bb.flushAcc()
	}
}

// AppendBits appends multiple bits from bytes.
// Bits are read MSB-first from the source data.
func (bb *BitBuffer) AppendBits(data []byte, numBits int) {
	for i := 0; i < numBits; i++ {
		byteIndex := i / 8
		bitIndex := i % 8

		// Extract bits MSB-first (bit 7, 6, 5, ..., 0)
		bit := (data[byteIndex] >> (7 - bitIndex)) & 1
		bb.AppendBit(int(bit))
	}
}

// AppendBitVector appends all bits from a BitVector.
func (bb *BitBuffer) AppendBitVector(bv *BitVector) {
	// Calculate number of bytes from bit length
	numBytes := (bv.length + 7) / 8

	// CCSDS MSB-first: bytes in order, bits within each byte from MSB to LSB
	for byteIdx := 0; byteIdx < numBytes; byteIdx++ {
		bitsInThisByte := 8

		// Last byte may have fewer than 8 bits
		if byteIdx == numBytes-1 {
			remainder := bv.length % 8
			if remainder != 0 {
				bitsInThisByte = remainder
			}
		}

		// With MSB-first bitvector indexing: bit 0 is MSB, bit 7 is LSB
		// We want to append bits in order: MSB first, LSB last
		startBit := byteIdx * 8
		for bitOffset := 0; bitOffset < bitsInThisByte; bitOffset++ {
			pos := startBit + bitOffset
			bit := bv.GetBit(pos)
			bb.AppendBit(bit)
		}
	}
}

// AppendBitVectorN appends the first n bits from a BitVector.
func (bb *BitBuffer) AppendBitVectorN(bv *BitVector, n int) {
	if n > bv.length {
		n = bv.length
	}
	for i := 0; i < n; i++ {
		bb.AppendBit(bv.GetBit(i))
	}
}

// AppendValue appends a value as count bits (MSB-first).
func (bb *BitBuffer) AppendValue(value uint64, count int) {
	if count <= 0 {
		return
	}
	// Mask to get only the bottom 'count' bits
	mask := uint64((1 << count) - 1)
	bb.acc = (bb.acc << count) | (value & mask)
	bb.accLen += count
	bb.numBits += count

	if bb.accLen >= 8 {
		bb.flushAcc()
	}
}

// ToBytes converts buffer contents to bytes.
func (bb *BitBuffer) ToBytes() []byte {
	if bb.numBits == 0 {
		return []byte{}
	}

	// Flush any remaining bits in accumulator
	numBytes := (bb.numBits + 7) / 8

	// Make sure we have enough space
	for len(bb.data) < numBytes {
		// Pad accumulator and flush
		if bb.accLen > 0 {
			// Pad to byte boundary
			padBits := 8 - (bb.accLen % 8)
			if padBits < 8 {
				bb.acc <<= padBits
				bb.accLen += padBits
			}
			bb.flushAcc()
		} else {
			bb.data = append(bb.data, 0)
		}
	}

	// Handle any remaining bits in accumulator (< 8 bits)
	if bb.accLen > 0 {
		// Pad to byte boundary and add
		padBits := 8 - bb.accLen
		lastByte := byte(bb.acc << padBits)
		if len(bb.data) < numBytes {
			bb.data = append(bb.data, lastByte)
		} else {
			bb.data[numBytes-1] = lastByte
		}
	}

	result := make([]byte, numBytes)
	copy(result, bb.data[:numBytes])
	return result
}

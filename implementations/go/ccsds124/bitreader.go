package ccsds124

import (
	"errors"
	"fmt"
)

// ErrEOF is returned when reading past the end of available data.
var ErrEOF = errors.New("no more bits to read")

// BitReader provides sequential bit-level reading from bytes.
//
// Bits are read MSB-first within each byte (matching BitBuffer output):
//   - First bit read is bit position 7 (MSB)
//   - Last bit read is bit position 0 (LSB)
type BitReader struct {
	data      []byte
	totalBits int
	position  int
}

// NewBitReader creates a new bit reader from bytes.
func NewBitReader(data []byte) *BitReader {
	return &BitReader{
		data:      data,
		totalBits: len(data) * 8,
		position:  0,
	}
}

// NewBitReaderWithBits creates a new bit reader with a specific bit count.
// This is useful when the data doesn't fill the last byte completely.
func NewBitReaderWithBits(data []byte, numBits int) *BitReader {
	maxBits := len(data) * 8
	if numBits > maxBits {
		numBits = maxBits
	}
	return &BitReader{
		data:      data,
		totalBits: numBits,
		position:  0,
	}
}

// Remaining returns the number of bits remaining to read.
func (br *BitReader) Remaining() int {
	return br.totalBits - br.position
}

// Position returns the current bit position.
func (br *BitReader) Position() int {
	return br.position
}

// PeekBit peeks at the next bit without consuming it.
func (br *BitReader) PeekBit() (int, error) {
	if br.position >= br.totalBits {
		return 0, ErrEOF
	}

	byteIndex := br.position / 8
	bitIndex := br.position % 8

	// MSB-first: bit 0 in stream is bit 7 of first byte
	bit := (br.data[byteIndex] >> (7 - bitIndex)) & 1
	return int(bit), nil
}

// ReadBit reads and consumes a single bit.
func (br *BitReader) ReadBit() (int, error) {
	bit, err := br.PeekBit()
	if err != nil {
		return 0, err
	}
	br.position++
	return bit, nil
}

// ReadBits reads and consumes multiple bits as an integer.
// Bits are returned MSB-first as an integer value.
func (br *BitReader) ReadBits(numBits int) (uint64, error) {
	if numBits == 0 {
		return 0, nil
	}

	if numBits > 64 {
		return 0, fmt.Errorf("cannot read more than 64 bits at once")
	}

	if br.position+numBits > br.totalBits {
		return 0, fmt.Errorf("not enough bits: need %d, have %d", numBits, br.Remaining())
	}

	var result uint64
	for i := 0; i < numBits; i++ {
		bit, err := br.ReadBit()
		if err != nil {
			return 0, err
		}
		result = (result << 1) | uint64(bit)
	}

	return result, nil
}

// AlignByte advances to the next byte boundary.
// If already at a byte boundary, does nothing.
func (br *BitReader) AlignByte() {
	bitOffset := br.position % 8
	if bitOffset != 0 {
		br.position += 8 - bitOffset
	}
}

// Skip advances the position by the given number of bits.
func (br *BitReader) Skip(numBits int) error {
	if br.position+numBits > br.totalBits {
		return fmt.Errorf("cannot skip %d bits: only %d remaining", numBits, br.Remaining())
	}
	br.position += numBits
	return nil
}

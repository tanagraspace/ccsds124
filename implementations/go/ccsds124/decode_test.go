package ccsds124

import (
	"testing"
)

func TestCountDecodeA1(t *testing.T) {
	// '0' -> 1
	bb := NewBitBuffer()
	bb.AppendBit(0)

	br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
	val, err := CountDecode(br)
	if err != nil {
		t.Errorf("CountDecode error: %v", err)
	}
	if val != 1 {
		t.Errorf("Expected 1, got %d", val)
	}
}

func TestCountDecodeTerminator(t *testing.T) {
	// '10' -> 0 (terminator)
	bb := NewBitBuffer()
	bb.AppendBit(1)
	bb.AppendBit(0)

	br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
	val, err := CountDecode(br)
	if err != nil {
		t.Errorf("CountDecode error: %v", err)
	}
	if val != 0 {
		t.Errorf("Expected 0 (terminator), got %d", val)
	}
}

func TestCountDecodeA2to33(t *testing.T) {
	// '110' + BIT5(value-2) -> value
	testCases := []int{2, 10, 20, 33}

	for _, expected := range testCases {
		bb := NewBitBuffer()
		CountEncode(bb, expected)

		br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
		val, err := CountDecode(br)
		if err != nil {
			t.Errorf("CountDecode(%d) error: %v", expected, err)
			continue
		}
		if val != expected {
			t.Errorf("Expected %d, got %d", expected, val)
		}
	}
}

func TestCountDecodeA34AndAbove(t *testing.T) {
	// '111' + BIT_E(value-2) -> value
	testCases := []int{34, 50, 100, 500, 1000, 10000}

	for _, expected := range testCases {
		bb := NewBitBuffer()
		CountEncode(bb, expected)

		br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
		val, err := CountDecode(br)
		if err != nil {
			t.Errorf("CountDecode(%d) error: %v", expected, err)
			continue
		}
		if val != expected {
			t.Errorf("Expected %d, got %d", expected, val)
		}
	}
}

func TestRLEDecodeAllZeros(t *testing.T) {
	// Encode all zeros, then decode
	bv, _ := NewBitVector(8)
	bv.Zero()

	bb := NewBitBuffer()
	RLEEncode(bb, bv)

	br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
	result, err := RLEDecode(br, 8)
	if err != nil {
		t.Errorf("RLEDecode error: %v", err)
	}

	if !result.Equals(bv) {
		t.Error("RLE round-trip failed for all zeros")
	}
}

func TestRLEDecodeSingleOne(t *testing.T) {
	// Encode single '1' at MSB, then decode
	bv, _ := NewBitVector(8)
	bv.SetBit(0, 1)

	bb := NewBitBuffer()
	RLEEncode(bb, bv)

	br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
	result, err := RLEDecode(br, 8)
	if err != nil {
		t.Errorf("RLEDecode error: %v", err)
	}

	if !result.Equals(bv) {
		t.Errorf("RLE round-trip failed: expected %v, got %v", bv.ToBytes(), result.ToBytes())
	}
}

func TestRLEDecodeMultipleOnes(t *testing.T) {
	// Encode pattern with multiple '1' bits, then decode
	bv, _ := NewBitVector(16)
	bv.SetBit(0, 1)
	bv.SetBit(5, 1)
	bv.SetBit(10, 1)
	bv.SetBit(15, 1)

	bb := NewBitBuffer()
	RLEEncode(bb, bv)

	br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
	result, err := RLEDecode(br, 16)
	if err != nil {
		t.Errorf("RLEDecode error: %v", err)
	}

	if !result.Equals(bv) {
		t.Errorf("RLE round-trip failed: expected %v, got %v", bv.ToBytes(), result.ToBytes())
	}
}

func TestRLEDecodeLargeVector(t *testing.T) {
	// Test with 720-bit vector (standard packet size)
	bv, _ := NewBitVector(720)
	bv.SetBit(0, 1)
	bv.SetBit(100, 1)
	bv.SetBit(359, 1)
	bv.SetBit(500, 1)
	bv.SetBit(719, 1)

	bb := NewBitBuffer()
	RLEEncode(bb, bv)

	br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
	result, err := RLEDecode(br, 720)
	if err != nil {
		t.Errorf("RLEDecode error: %v", err)
	}

	if !result.Equals(bv) {
		t.Error("RLE round-trip failed for 720-bit vector")
	}
}

func TestBitInsert(t *testing.T) {
	// Create data with extracted bits
	bb := NewBitBuffer()
	bb.AppendBit(1) // For position 6
	bb.AppendBit(0) // For position 4
	bb.AppendBit(0) // For position 1

	mask, _ := NewBitVector(8)
	mask.FromBytes([]byte{0x4A}) // 01001010 - positions 1, 4, 6

	data, _ := NewBitVector(8)
	data.Zero()

	br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
	err := BitInsert(br, data, mask)
	if err != nil {
		t.Errorf("BitInsert error: %v", err)
	}

	// Check the inserted bits
	// Position 6 should be 1, positions 1 and 4 should be 0
	if data.GetBit(6) != 1 {
		t.Errorf("Position 6 should be 1, got %d", data.GetBit(6))
	}
	if data.GetBit(4) != 0 {
		t.Errorf("Position 4 should be 0, got %d", data.GetBit(4))
	}
	if data.GetBit(1) != 0 {
		t.Errorf("Position 1 should be 0, got %d", data.GetBit(1))
	}
}

func TestBitInsertForward(t *testing.T) {
	// Create data with bits in forward order
	bb := NewBitBuffer()
	bb.AppendBit(0) // For position 1
	bb.AppendBit(1) // For position 4
	bb.AppendBit(1) // For position 6

	mask, _ := NewBitVector(8)
	mask.FromBytes([]byte{0x4A}) // 01001010 - positions 1, 4, 6

	data, _ := NewBitVector(8)
	data.Zero()

	br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
	err := BitInsertForward(br, data, mask)
	if err != nil {
		t.Errorf("BitInsertForward error: %v", err)
	}

	// Check the inserted bits
	if data.GetBit(1) != 0 {
		t.Errorf("Position 1 should be 0, got %d", data.GetBit(1))
	}
	if data.GetBit(4) != 1 {
		t.Errorf("Position 4 should be 1, got %d", data.GetBit(4))
	}
	if data.GetBit(6) != 1 {
		t.Errorf("Position 6 should be 1, got %d", data.GetBit(6))
	}
}

func TestBitExtractInsertRoundTrip(t *testing.T) {
	// Extract bits, then insert them back
	originalData, _ := NewBitVector(8)
	originalData.FromBytes([]byte{0xB3}) // 10110011

	mask, _ := NewBitVector(8)
	mask.FromBytes([]byte{0x4A}) // 01001010

	// Extract
	bbExtract := NewBitBuffer()
	BitExtract(bbExtract, originalData, mask)

	// Insert into new vector
	newData, _ := NewBitVector(8)
	newData.Zero()

	br := NewBitReaderWithBits(bbExtract.ToBytes(), bbExtract.NumBits())
	err := BitInsert(br, newData, mask)
	if err != nil {
		t.Errorf("BitInsert error: %v", err)
	}

	// Compare only the masked positions
	for i := 0; i < 8; i++ {
		if mask.GetBit(i) != 0 {
			if originalData.GetBit(i) != newData.GetBit(i) {
				t.Errorf("Position %d: expected %d, got %d",
					i, originalData.GetBit(i), newData.GetBit(i))
			}
		}
	}
}

func TestBitExtractForwardInsertForwardRoundTrip(t *testing.T) {
	// Extract forward, then insert forward
	originalData, _ := NewBitVector(8)
	originalData.FromBytes([]byte{0xAB}) // 10101011

	mask, _ := NewBitVector(8)
	mask.FromBytes([]byte{0xFF}) // All ones

	// Extract forward
	bbExtract := NewBitBuffer()
	BitExtractForward(bbExtract, originalData, mask)

	// Insert forward into new vector
	newData, _ := NewBitVector(8)
	newData.Zero()

	br := NewBitReaderWithBits(bbExtract.ToBytes(), bbExtract.NumBits())
	err := BitInsertForward(br, newData, mask)
	if err != nil {
		t.Errorf("BitInsertForward error: %v", err)
	}

	// Should be identical
	if !originalData.Equals(newData) {
		t.Errorf("Forward round-trip failed: expected %v, got %v",
			originalData.ToBytes(), newData.ToBytes())
	}
}

func TestCountEncodeDecodeRoundTrip(t *testing.T) {
	// Test round-trip for various values
	testCases := []int{1, 2, 5, 10, 20, 33, 34, 50, 100, 500, 1000, 5000, 10000, 50000}

	for _, expected := range testCases {
		bb := NewBitBuffer()
		err := CountEncode(bb, expected)
		if err != nil {
			t.Errorf("CountEncode(%d) error: %v", expected, err)
			continue
		}

		br := NewBitReaderWithBits(bb.ToBytes(), bb.NumBits())
		val, err := CountDecode(br)
		if err != nil {
			t.Errorf("CountDecode(%d) error: %v", expected, err)
			continue
		}

		if val != expected {
			t.Errorf("Round-trip failed for %d: got %d", expected, val)
		}
	}
}

func TestDecodeErrors(t *testing.T) {
	// Test with empty reader
	br := NewBitReader([]byte{})

	_, err := CountDecode(br)
	if err == nil {
		t.Error("Expected error for empty reader")
	}

	_, err = RLEDecode(br, 8)
	if err == nil {
		t.Error("Expected error for empty reader in RLEDecode")
	}
}

func TestBitInsertErrors(t *testing.T) {
	bb := NewBitBuffer()
	br := NewBitReader(bb.ToBytes())

	// Nil inputs
	err := BitInsert(br, nil, nil)
	if err == nil {
		t.Error("Expected error for nil inputs")
	}

	// Length mismatch
	data, _ := NewBitVector(8)
	mask, _ := NewBitVector(16)
	err = BitInsert(br, data, mask)
	if err == nil {
		t.Error("Expected error for length mismatch")
	}
}

func TestBitInsertForwardErrors(t *testing.T) {
	bb := NewBitBuffer()
	br := NewBitReader(bb.ToBytes())

	// Nil inputs
	err := BitInsertForward(br, nil, nil)
	if err == nil {
		t.Error("Expected error for nil inputs")
	}

	// Length mismatch
	data, _ := NewBitVector(8)
	mask, _ := NewBitVector(16)
	err = BitInsertForward(br, data, mask)
	if err == nil {
		t.Error("Expected error for length mismatch")
	}

	// Not enough bits to read
	data2, _ := NewBitVector(8)
	mask2, _ := NewBitVector(8)
	mask2.FromBytes([]byte{0xFF})     // All ones - needs 8 bits
	br2 := NewBitReader([]byte{0x0F}) // Only provides partial data
	br2.ReadBits(5)                   // Consume some bits
	err = BitInsertForward(br2, data2, mask2)
	if err == nil {
		t.Error("Expected error when not enough bits available")
	}
}

func TestCountDecodeInsufficientBits(t *testing.T) {
	// Test '11' prefix without enough bits for third bit
	bb := NewBitBuffer()
	bb.AppendBit(1)
	bb.AppendBit(1)
	// Missing third bit

	br := NewBitReaderWithBits(bb.ToBytes(), 2)
	_, err := CountDecode(br)
	if err == nil {
		t.Error("Expected error for insufficient bits after '11' prefix")
	}
}

func TestCountDecodeBIT5Insufficient(t *testing.T) {
	// Test '110' prefix without enough bits for BIT5
	bb := NewBitBuffer()
	bb.AppendBit(1)
	bb.AppendBit(1)
	bb.AppendBit(0)
	bb.AppendBit(0) // Only 1 of 5 bits
	// Missing 4 more bits

	br := NewBitReaderWithBits(bb.ToBytes(), 4)
	_, err := CountDecode(br)
	if err == nil {
		t.Error("Expected error for insufficient BIT5 bits")
	}
}

func TestRLEDecodeInsufficientBits(t *testing.T) {
	// Create a partial RLE encoding that ends abruptly
	bb := NewBitBuffer()
	bb.AppendBit(0) // COUNT=1
	// Missing terminator

	br := NewBitReaderWithBits(bb.ToBytes(), 1)
	_, err := RLEDecode(br, 8)
	if err == nil {
		t.Error("Expected error for incomplete RLE encoding")
	}
}

func TestBitInsertInsufficientBits(t *testing.T) {
	// Create mask with many set bits but few bits to read
	bb := NewBitBuffer()
	bb.AppendBit(1)
	// Only one bit available

	mask, _ := NewBitVector(8)
	mask.FromBytes([]byte{0xFF}) // All 8 bits set

	data, _ := NewBitVector(8)

	br := NewBitReaderWithBits(bb.ToBytes(), 1)
	err := BitInsert(br, data, mask)
	if err == nil {
		t.Error("Expected error for insufficient bits")
	}
}

// TestRLEDecodeRejectsInvalidDelta verifies GOTCHAS #20 / issue #92:
// an RLE delta exceeding the remaining bit position must be rejected,
// not silently skipped.
func TestRLEDecodeRejectsInvalidDelta(t *testing.T) {
	// COUNT(9) = '110'+BIT5(7), then terminator '10' -> 0xC7 0x80.
	// Delta 9 exceeds an 8-bit vector.
	br := NewBitReader([]byte{0xC7, 0x80})
	if _, err := RLEDecode(br, 8); err == nil {
		t.Error("RLEDecode should reject delta beyond bit position")
	}
}

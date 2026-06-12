package ccsds124

import (
	"testing"
)

func TestNewBitReader(t *testing.T) {
	data := []byte{0xAB, 0xCD}
	br := NewBitReader(data)

	if br.Remaining() != 16 {
		t.Errorf("Expected 16 bits, got %d", br.Remaining())
	}
	if br.Position() != 0 {
		t.Errorf("Expected position 0, got %d", br.Position())
	}
}

func TestNewBitReaderWithBits(t *testing.T) {
	data := []byte{0xFF, 0xFF}
	br := NewBitReaderWithBits(data, 12)

	if br.Remaining() != 12 {
		t.Errorf("Expected 12 bits, got %d", br.Remaining())
	}
}

func TestBitReaderReadBit(t *testing.T) {
	// 0xAA = 10101010
	data := []byte{0xAA}
	br := NewBitReader(data)

	expected := []int{1, 0, 1, 0, 1, 0, 1, 0}
	for i, exp := range expected {
		bit, err := br.ReadBit()
		if err != nil {
			t.Errorf("ReadBit %d error: %v", i, err)
		}
		if bit != exp {
			t.Errorf("Bit %d: expected %d, got %d", i, exp, bit)
		}
	}

	// Should be at end
	_, err := br.ReadBit()
	if err == nil {
		t.Error("Expected EOF error")
	}
}

func TestBitReaderPeekBit(t *testing.T) {
	data := []byte{0x80} // 10000000
	br := NewBitReader(data)

	// Peek should not advance position
	bit1, _ := br.PeekBit()
	bit2, _ := br.PeekBit()

	if bit1 != bit2 {
		t.Error("PeekBit should not advance position")
	}
	if br.Position() != 0 {
		t.Error("Position should still be 0 after peek")
	}
}

func TestBitReaderReadBits(t *testing.T) {
	data := []byte{0xAB, 0xCD}
	br := NewBitReader(data)

	// Read 8 bits
	val, err := br.ReadBits(8)
	if err != nil {
		t.Errorf("ReadBits error: %v", err)
	}
	if val != 0xAB {
		t.Errorf("Expected 0xAB, got 0x%02X", val)
	}

	// Read remaining 8 bits
	val, err = br.ReadBits(8)
	if err != nil {
		t.Errorf("ReadBits error: %v", err)
	}
	if val != 0xCD {
		t.Errorf("Expected 0xCD, got 0x%02X", val)
	}
}

func TestBitReaderReadBitsPartial(t *testing.T) {
	data := []byte{0xF0} // 11110000
	br := NewBitReader(data)

	// Read 4 bits
	val, err := br.ReadBits(4)
	if err != nil {
		t.Errorf("ReadBits error: %v", err)
	}
	if val != 0x0F { // 1111
		t.Errorf("Expected 0x0F, got 0x%02X", val)
	}

	// Read next 4 bits
	val, err = br.ReadBits(4)
	if err != nil {
		t.Errorf("ReadBits error: %v", err)
	}
	if val != 0x00 { // 0000
		t.Errorf("Expected 0x00, got 0x%02X", val)
	}
}

func TestBitReaderReadBitsZero(t *testing.T) {
	br := NewBitReader([]byte{0xFF})

	val, err := br.ReadBits(0)
	if err != nil {
		t.Errorf("ReadBits(0) error: %v", err)
	}
	if val != 0 {
		t.Errorf("ReadBits(0) should return 0, got %d", val)
	}
	if br.Position() != 0 {
		t.Error("ReadBits(0) should not advance position")
	}
}

func TestBitReaderReadBitsOverflow(t *testing.T) {
	br := NewBitReader([]byte{0xFF})

	_, err := br.ReadBits(16)
	if err == nil {
		t.Error("Expected error when reading more bits than available")
	}
}

func TestBitReaderAlignByte(t *testing.T) {
	br := NewBitReader([]byte{0xFF, 0x00})

	// Read 3 bits
	br.ReadBits(3)
	if br.Position() != 3 {
		t.Errorf("Expected position 3, got %d", br.Position())
	}

	// Align to byte
	br.AlignByte()
	if br.Position() != 8 {
		t.Errorf("Expected position 8 after align, got %d", br.Position())
	}

	// Align when already at byte boundary should do nothing
	br.AlignByte()
	if br.Position() != 8 {
		t.Errorf("Expected position 8 after second align, got %d", br.Position())
	}
}

func TestBitReaderSkip(t *testing.T) {
	br := NewBitReader([]byte{0xFF, 0x00, 0xAA})

	// Skip 8 bits
	err := br.Skip(8)
	if err != nil {
		t.Errorf("Skip error: %v", err)
	}
	if br.Position() != 8 {
		t.Errorf("Expected position 8, got %d", br.Position())
	}

	// Skip 4 more bits
	err = br.Skip(4)
	if err != nil {
		t.Errorf("Skip error: %v", err)
	}
	if br.Position() != 12 {
		t.Errorf("Expected position 12, got %d", br.Position())
	}
}

func TestBitReaderSkipOverflow(t *testing.T) {
	br := NewBitReader([]byte{0xFF})

	err := br.Skip(16)
	if err == nil {
		t.Error("Expected error when skipping more bits than available")
	}
}

func TestBitReaderMSBFirst(t *testing.T) {
	// Verify MSB-first ordering
	// 0x80 = 10000000, first bit should be 1
	br := NewBitReader([]byte{0x80})

	bit, _ := br.ReadBit()
	if bit != 1 {
		t.Error("MSB-first: first bit of 0x80 should be 1")
	}

	// 0x01 = 00000001, first bit should be 0, last should be 1
	br = NewBitReader([]byte{0x01})

	for i := 0; i < 7; i++ {
		bit, _ := br.ReadBit()
		if bit != 0 {
			t.Errorf("MSB-first: bit %d of 0x01 should be 0", i)
		}
	}
	bit, _ = br.ReadBit()
	if bit != 1 {
		t.Error("MSB-first: last bit of 0x01 should be 1")
	}
}

func TestBitReaderRoundTrip(t *testing.T) {
	// Write to BitBuffer, read back with BitReader
	bb := NewBitBuffer()
	bb.AppendValue(0x12345678, 32)
	bb.AppendBit(1)
	bb.AppendBit(0)
	bb.AppendBit(1)

	data := bb.ToBytes()
	br := NewBitReaderWithBits(data, bb.NumBits())

	val, _ := br.ReadBits(32)
	if val != 0x12345678 {
		t.Errorf("Round-trip: expected 0x12345678, got 0x%08X", val)
	}

	bit, _ := br.ReadBit()
	if bit != 1 {
		t.Error("Round-trip: expected bit 1")
	}
	bit, _ = br.ReadBit()
	if bit != 0 {
		t.Error("Round-trip: expected bit 0")
	}
	bit, _ = br.ReadBit()
	if bit != 1 {
		t.Error("Round-trip: expected bit 1")
	}
}

func TestNewBitReaderWithBitsExceedsData(t *testing.T) {
	// Request more bits than data contains - should clamp
	data := []byte{0xFF, 0xAA} // 16 bits
	br := NewBitReaderWithBits(data, 100)

	// Should clamp to 16 bits
	if br.Remaining() != 16 {
		t.Errorf("Expected 16 bits (clamped), got %d", br.Remaining())
	}
}

func TestBitReaderReadBitsLarge(t *testing.T) {
	// Test reading many bits at once
	data := []byte{0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE}
	br := NewBitReader(data)

	val, err := br.ReadBits(64)
	if err != nil {
		t.Errorf("ReadBits(64) error: %v", err)
	}
	if val != 0xDEADBEEFCAFEBABE {
		t.Errorf("Expected 0xDEADBEEFCAFEBABE, got 0x%016X", val)
	}
}

func TestBitReaderPosition(t *testing.T) {
	data := []byte{0xFF}
	br := NewBitReader(data)

	if br.Position() != 0 {
		t.Errorf("Initial position should be 0, got %d", br.Position())
	}

	br.ReadBit()
	if br.Position() != 1 {
		t.Errorf("Position after ReadBit should be 1, got %d", br.Position())
	}

	br.ReadBits(4)
	if br.Position() != 5 {
		t.Errorf("Position after ReadBits(4) should be 5, got %d", br.Position())
	}
}

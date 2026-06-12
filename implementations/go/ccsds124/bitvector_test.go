package ccsds124

import (
	"bytes"
	"testing"
)

func TestNewBitVector(t *testing.T) {
	// Valid cases
	bv, err := NewBitVector(8)
	if err != nil {
		t.Errorf("NewBitVector(8) error: %v", err)
	}
	if bv.Length() != 8 {
		t.Errorf("Expected length 8, got %d", bv.Length())
	}

	bv, err = NewBitVector(720)
	if err != nil {
		t.Errorf("NewBitVector(720) error: %v", err)
	}
	if bv.Length() != 720 {
		t.Errorf("Expected length 720, got %d", bv.Length())
	}

	// Invalid cases
	_, err = NewBitVector(0)
	if err == nil {
		t.Error("Expected error for NewBitVector(0)")
	}

	_, err = NewBitVector(-1)
	if err == nil {
		t.Error("Expected error for NewBitVector(-1)")
	}
}

func TestBitVectorGetSetBit(t *testing.T) {
	bv, _ := NewBitVector(16)

	// All bits should be 0 initially
	for i := 0; i < 16; i++ {
		if bv.GetBit(i) != 0 {
			t.Errorf("Bit %d should be 0 initially", i)
		}
	}

	// Set some bits
	bv.SetBit(0, 1) // MSB
	bv.SetBit(7, 1)
	bv.SetBit(15, 1) // LSB

	if bv.GetBit(0) != 1 {
		t.Error("Bit 0 should be 1")
	}
	if bv.GetBit(7) != 1 {
		t.Error("Bit 7 should be 1")
	}
	if bv.GetBit(15) != 1 {
		t.Error("Bit 15 should be 1")
	}
	if bv.GetBit(1) != 0 {
		t.Error("Bit 1 should be 0")
	}

	// Clear a bit
	bv.SetBit(7, 0)
	if bv.GetBit(7) != 0 {
		t.Error("Bit 7 should be 0 after clearing")
	}
}

func TestBitVectorFromToBytes(t *testing.T) {
	// Test with 2 bytes
	bv, _ := NewBitVector(16)
	bv.FromBytes([]byte{0xAB, 0xCD})
	result := bv.ToBytes()
	if !bytes.Equal(result, []byte{0xAB, 0xCD}) {
		t.Errorf("Expected [0xAB, 0xCD], got %v", result)
	}

	// Test with 4 bytes (full word)
	bv, _ = NewBitVector(32)
	bv.FromBytes([]byte{0x12, 0x34, 0x56, 0x78})
	result = bv.ToBytes()
	if !bytes.Equal(result, []byte{0x12, 0x34, 0x56, 0x78}) {
		t.Errorf("Expected [0x12, 0x34, 0x56, 0x78], got %v", result)
	}

	// Test with 90 bytes (720 bits - standard packet size)
	bv, _ = NewBitVector(720)
	data := make([]byte, 90)
	for i := range data {
		data[i] = byte(i)
	}
	bv.FromBytes(data)
	result = bv.ToBytes()
	if !bytes.Equal(result, data) {
		t.Error("Round-trip failed for 90 bytes")
	}
}

func TestBitVectorCopy(t *testing.T) {
	bv, _ := NewBitVector(16)
	bv.FromBytes([]byte{0xAB, 0xCD})

	bv2 := bv.Copy()
	if !bv.Equals(bv2) {
		t.Error("Copy should be equal to original")
	}

	// Modify copy, original should not change
	bv2.SetBit(0, 0)
	if bv.GetBit(0) != 1 {
		t.Error("Original should not be affected by copy modification")
	}
}

func TestBitVectorCopyFrom(t *testing.T) {
	bv1, _ := NewBitVector(16)
	bv2, _ := NewBitVector(16)

	bv1.FromBytes([]byte{0xAB, 0xCD})
	bv2.CopyFrom(bv1)

	if !bv1.Equals(bv2) {
		t.Error("CopyFrom should make vectors equal")
	}
}

func TestBitVectorXOR(t *testing.T) {
	a, _ := NewBitVector(16)
	b, _ := NewBitVector(16)

	a.FromBytes([]byte{0xFF, 0x00})
	b.FromBytes([]byte{0x0F, 0xF0})

	result := a.XOR(b)
	expected := []byte{0xF0, 0xF0}
	if !bytes.Equal(result.ToBytes(), expected) {
		t.Errorf("XOR result incorrect: got %v, expected %v", result.ToBytes(), expected)
	}
}

func TestBitVectorXORInto(t *testing.T) {
	a, _ := NewBitVector(16)
	b, _ := NewBitVector(16)
	result, _ := NewBitVector(16)

	a.FromBytes([]byte{0xFF, 0x00})
	b.FromBytes([]byte{0x0F, 0xF0})

	result.XORInto(a, b)
	expected := []byte{0xF0, 0xF0}
	if !bytes.Equal(result.ToBytes(), expected) {
		t.Errorf("XORInto result incorrect: got %v, expected %v", result.ToBytes(), expected)
	}
}

func TestBitVectorOR(t *testing.T) {
	a, _ := NewBitVector(16)
	b, _ := NewBitVector(16)

	a.FromBytes([]byte{0xF0, 0x00})
	b.FromBytes([]byte{0x0F, 0xF0})

	result := a.OR(b)
	expected := []byte{0xFF, 0xF0}
	if !bytes.Equal(result.ToBytes(), expected) {
		t.Errorf("OR result incorrect: got %v, expected %v", result.ToBytes(), expected)
	}
}

func TestBitVectorORInto(t *testing.T) {
	a, _ := NewBitVector(16)
	b, _ := NewBitVector(16)
	result, _ := NewBitVector(16)

	a.FromBytes([]byte{0xF0, 0x00})
	b.FromBytes([]byte{0x0F, 0xF0})

	result.ORInto(a, b)
	expected := []byte{0xFF, 0xF0}
	if !bytes.Equal(result.ToBytes(), expected) {
		t.Errorf("ORInto result incorrect: got %v, expected %v", result.ToBytes(), expected)
	}
}

func TestBitVectorAND(t *testing.T) {
	a, _ := NewBitVector(16)
	b, _ := NewBitVector(16)

	a.FromBytes([]byte{0xFF, 0x0F})
	b.FromBytes([]byte{0x0F, 0xFF})

	result := a.AND(b)
	expected := []byte{0x0F, 0x0F}
	if !bytes.Equal(result.ToBytes(), expected) {
		t.Errorf("AND result incorrect: got %v, expected %v", result.ToBytes(), expected)
	}
}

func TestBitVectorNOT(t *testing.T) {
	bv, _ := NewBitVector(16)
	bv.FromBytes([]byte{0xF0, 0x0F})

	result := bv.NOT()
	expected := []byte{0x0F, 0xF0}
	if !bytes.Equal(result.ToBytes(), expected) {
		t.Errorf("NOT result incorrect: got %v, expected %v", result.ToBytes(), expected)
	}
}

func TestBitVectorNOTPartialByte(t *testing.T) {
	// Test NOT with non-byte-aligned length (12 bits)
	bv, _ := NewBitVector(12)
	// Set all bits to 0
	bv.Zero()

	// NOT should give all 1s, but only for 12 bits
	result := bv.NOT()

	// First byte should be all 1s
	// Second byte should only have top 4 bits set (0xF0)
	expected := []byte{0xFF, 0xF0}
	if !bytes.Equal(result.ToBytes(), expected) {
		t.Errorf("NOT partial byte: got %v, expected %v", result.ToBytes(), expected)
	}
}

func TestBitVectorLeftShift(t *testing.T) {
	bv, _ := NewBitVector(8)
	bv.FromBytes([]byte{0x81}) // 10000001

	result := bv.LeftShift()
	// After left shift: 00000010 (MSB lost, LSB = 0)
	expected := []byte{0x02}
	if !bytes.Equal(result.ToBytes(), expected) {
		t.Errorf("LeftShift result incorrect: got %v, expected %v", result.ToBytes(), expected)
	}
}

func TestBitVectorLeftShiftMultiWord(t *testing.T) {
	bv, _ := NewBitVector(64)
	// Set bit at position 32 (start of second word)
	bv.SetBit(32, 1)

	result := bv.LeftShift()
	// After left shift, bit should move to position 31
	if result.GetBit(31) != 1 {
		t.Error("Left shift should move bit from 32 to 31")
	}
	if result.GetBit(32) != 0 {
		t.Error("Position 32 should be 0 after shift")
	}
}

func TestBitVectorReverse(t *testing.T) {
	bv, _ := NewBitVector(8)
	bv.FromBytes([]byte{0xF0}) // 11110000

	result := bv.Reverse()
	expected := []byte{0x0F} // 00001111
	if !bytes.Equal(result.ToBytes(), expected) {
		t.Errorf("Reverse result incorrect: got %v, expected %v", result.ToBytes(), expected)
	}
}

func TestBitVectorHammingWeight(t *testing.T) {
	bv, _ := NewBitVector(16)

	// All zeros
	if bv.HammingWeight() != 0 {
		t.Errorf("Hamming weight of zeros should be 0, got %d", bv.HammingWeight())
	}

	// All ones (16 bits)
	bv.FromBytes([]byte{0xFF, 0xFF})
	if bv.HammingWeight() != 16 {
		t.Errorf("Hamming weight of 16 ones should be 16, got %d", bv.HammingWeight())
	}

	// Mixed
	bv.FromBytes([]byte{0xAA, 0x55}) // 10101010 01010101 = 8 ones
	if bv.HammingWeight() != 8 {
		t.Errorf("Hamming weight should be 8, got %d", bv.HammingWeight())
	}
}

func TestBitVectorEquals(t *testing.T) {
	a, _ := NewBitVector(16)
	b, _ := NewBitVector(16)

	a.FromBytes([]byte{0xAB, 0xCD})
	b.FromBytes([]byte{0xAB, 0xCD})

	if !a.Equals(b) {
		t.Error("Equal vectors should be equal")
	}

	b.SetBit(0, 0)
	if a.Equals(b) {
		t.Error("Different vectors should not be equal")
	}

	// Different lengths
	c, _ := NewBitVector(8)
	if a.Equals(c) {
		t.Error("Vectors of different lengths should not be equal")
	}
}

func TestBitVectorZero(t *testing.T) {
	bv, _ := NewBitVector(16)
	bv.FromBytes([]byte{0xFF, 0xFF})

	bv.Zero()

	for i := 0; i < 16; i++ {
		if bv.GetBit(i) != 0 {
			t.Errorf("Bit %d should be 0 after Zero()", i)
		}
	}
}

func TestBitVectorOutOfBoundsAccess(t *testing.T) {
	bv, _ := NewBitVector(8)

	// GetBit out of bounds should return 0
	if bv.GetBit(-1) != 0 {
		t.Error("GetBit(-1) should return 0")
	}
	if bv.GetBit(8) != 0 {
		t.Error("GetBit(8) should return 0")
	}

	// SetBit out of bounds should be no-op
	bv.SetBit(-1, 1)
	bv.SetBit(8, 1)
	// Should not panic, and no bits should be set
	for i := 0; i < 8; i++ {
		if bv.GetBit(i) != 0 {
			t.Errorf("Out of bounds SetBit should not affect valid bits")
		}
	}
}

func TestBitVectorCopyFromDifferentSizes(t *testing.T) {
	// Copy from smaller to larger (different numWords: 64-bit = 2 words, 32-bit = 1 word)
	large, _ := NewBitVector(64)
	large.FromBytes([]byte{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})

	small, _ := NewBitVector(32)
	small.FromBytes([]byte{0xAB, 0xCD, 0xEF, 0x12})

	large.CopyFrom(small)
	// First word should be copied from small
	result := large.ToBytes()
	if result[0] != 0xAB {
		t.Errorf("CopyFrom smaller: expected 0xAB at position 0, got 0x%02X", result[0])
	}
}

func TestBitVectorXORDifferentSizes(t *testing.T) {
	// 64-bit = 2 words, 32-bit = 1 word
	large, _ := NewBitVector(64)
	large.FromBytes([]byte{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})

	small, _ := NewBitVector(32)
	small.FromBytes([]byte{0x0F, 0x0F, 0x0F, 0x0F})

	// XOR large with small - should use min(numWords)
	result := large.XOR(small)
	if result.Length() != 64 {
		t.Errorf("XOR result should have length 64, got %d", result.Length())
	}
}

func TestBitVectorXORIntoDifferentSizes(t *testing.T) {
	// dest = 96 bits (3 words), a = 64 bits (2 words), b = 32 bits (1 word)
	dest, _ := NewBitVector(96)
	a, _ := NewBitVector(64)
	b, _ := NewBitVector(32)

	a.FromBytes([]byte{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00})
	b.FromBytes([]byte{0x0F, 0x0F, 0x0F, 0x0F})

	dest.XORInto(a, b)
	// Should use minimum of all three numWords
	if dest.Length() != 96 {
		t.Errorf("XORInto result should have length 96, got %d", dest.Length())
	}
}

func TestBitVectorORDifferentSizes(t *testing.T) {
	// 64-bit = 2 words, 32-bit = 1 word
	large, _ := NewBitVector(64)
	large.FromBytes([]byte{0xF0, 0x00, 0xF0, 0x00, 0xF0, 0x00, 0xF0, 0x00})

	small, _ := NewBitVector(32)
	small.FromBytes([]byte{0x0F, 0x0F, 0x0F, 0x0F})

	result := large.OR(small)
	if result.Length() != 64 {
		t.Errorf("OR result should have length 64, got %d", result.Length())
	}
}

func TestBitVectorORIntoDifferentSizes(t *testing.T) {
	// dest = 96 bits (3 words), a = 64 bits (2 words), b = 32 bits (1 word)
	dest, _ := NewBitVector(96)
	a, _ := NewBitVector(64)
	b, _ := NewBitVector(32)

	a.FromBytes([]byte{0xF0, 0x00, 0xF0, 0x00, 0xF0, 0x00, 0xF0, 0x00})
	b.FromBytes([]byte{0x0F, 0x0F, 0x0F, 0x0F})

	dest.ORInto(a, b)
	if dest.Length() != 96 {
		t.Errorf("ORInto result should have length 96, got %d", dest.Length())
	}
}

func TestBitVectorANDDifferentSizes(t *testing.T) {
	// 64-bit = 2 words, 32-bit = 1 word
	large, _ := NewBitVector(64)
	large.FromBytes([]byte{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})

	small, _ := NewBitVector(32)
	small.FromBytes([]byte{0x0F, 0x0F, 0x0F, 0x0F})

	result := large.AND(small)
	if result.Length() != 64 {
		t.Errorf("AND result should have length 64, got %d", result.Length())
	}
}

func TestBitVectorHammingWeightNonByteAligned(t *testing.T) {
	// Test with 12 bits (not byte-aligned) to cover extraBits branch
	bv, _ := NewBitVector(12)
	bv.FromBytes([]byte{0xFF, 0xF0}) // All 12 bits set to 1

	hw := bv.HammingWeight()
	if hw != 12 {
		t.Errorf("HammingWeight of 12 set bits should be 12, got %d", hw)
	}
}

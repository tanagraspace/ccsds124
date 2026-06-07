package pocketplus

import (
	"bytes"
	"testing"
)

func TestUpdateBuildInitial(t *testing.T) {
	build, _ := NewBitVector(8)
	input, _ := NewBitVector(8)
	prevInput, _ := NewBitVector(8)

	input.FromBytes([]byte{0xFF})
	prevInput.FromBytes([]byte{0x00})

	// At t=0, build should be zero regardless of inputs
	UpdateBuild(build, input, prevInput, false, 0)

	expected, _ := NewBitVector(8)
	expected.Zero()
	if !build.Equals(expected) {
		t.Errorf("t=0: build should be zero, got %v", build.ToBytes())
	}
}

func TestUpdateBuildWithNewMaskFlag(t *testing.T) {
	build, _ := NewBitVector(8)
	build.FromBytes([]byte{0xFF}) // Non-zero build

	input, _ := NewBitVector(8)
	prevInput, _ := NewBitVector(8)
	input.FromBytes([]byte{0xAA})
	prevInput.FromBytes([]byte{0x55})

	// With newMaskFlag=true, build should be reset to zero
	UpdateBuild(build, input, prevInput, true, 5)

	expected, _ := NewBitVector(8)
	expected.Zero()
	if !build.Equals(expected) {
		t.Errorf("newMaskFlag: build should be zero, got %v", build.ToBytes())
	}
}

func TestUpdateBuildAccumulation(t *testing.T) {
	build, _ := NewBitVector(8)
	build.FromBytes([]byte{0x0F}) // Lower nibble set

	input, _ := NewBitVector(8)
	prevInput, _ := NewBitVector(8)
	input.FromBytes([]byte{0xF0})     // Upper nibble
	prevInput.FromBytes([]byte{0x00}) // All zeros

	// Changes = F0 XOR 00 = F0
	// Build = F0 OR 0F = FF
	UpdateBuild(build, input, prevInput, false, 1)

	if !bytes.Equal(build.ToBytes(), []byte{0xFF}) {
		t.Errorf("Expected 0xFF, got %v", build.ToBytes())
	}
}

func TestUpdateMaskWithoutNewMaskFlag(t *testing.T) {
	mask, _ := NewBitVector(8)
	mask.FromBytes([]byte{0x0F}) // Lower nibble set

	input, _ := NewBitVector(8)
	prevInput, _ := NewBitVector(8)
	buildPrev, _ := NewBitVector(8)

	input.FromBytes([]byte{0xF0})     // Upper nibble
	prevInput.FromBytes([]byte{0x00}) // All zeros
	buildPrev.FromBytes([]byte{0xAA}) // Ignored when newMaskFlag=false

	// Changes = F0 XOR 00 = F0
	// Mask = F0 OR 0F = FF
	UpdateMask(mask, input, prevInput, buildPrev, false)

	if !bytes.Equal(mask.ToBytes(), []byte{0xFF}) {
		t.Errorf("Expected 0xFF, got %v", mask.ToBytes())
	}
}

func TestUpdateMaskWithNewMaskFlag(t *testing.T) {
	mask, _ := NewBitVector(8)
	mask.FromBytes([]byte{0xFF}) // All ones

	input, _ := NewBitVector(8)
	prevInput, _ := NewBitVector(8)
	buildPrev, _ := NewBitVector(8)

	input.FromBytes([]byte{0x0F})     // Lower nibble
	prevInput.FromBytes([]byte{0x00}) // All zeros
	buildPrev.FromBytes([]byte{0xF0}) // Upper nibble

	// Changes = 0F XOR 00 = 0F
	// Mask = 0F OR F0 = FF
	UpdateMask(mask, input, prevInput, buildPrev, true)

	if !bytes.Equal(mask.ToBytes(), []byte{0xFF}) {
		t.Errorf("Expected 0xFF, got %v", mask.ToBytes())
	}
}

func TestComputeChangeInitial(t *testing.T) {
	change, _ := NewBitVector(8)
	change.FromBytes([]byte{0xFF}) // Should be overwritten
	mask, _ := NewBitVector(8)
	prevMask, _ := NewBitVector(8)

	mask.FromBytes([]byte{0xAB})
	prevMask.FromBytes([]byte{0xFF}) // Should be ignored at t=0

	// CCSDS Eq. 8: D0 = 0 regardless of M0 (no change at initialization)
	ComputeChange(change, mask, prevMask, 0)

	expected, _ := NewBitVector(8)
	expected.Zero()
	if !change.Equals(expected) {
		t.Errorf("t=0: change should be zero per CCSDS Eq. 8, got %v", change.ToBytes())
	}
}

func TestComputeChangeSubsequent(t *testing.T) {
	change, _ := NewBitVector(8)
	mask, _ := NewBitVector(8)
	prevMask, _ := NewBitVector(8)

	mask.FromBytes([]byte{0xFF})
	prevMask.FromBytes([]byte{0x0F})

	// Change = FF XOR 0F = F0
	ComputeChange(change, mask, prevMask, 1)

	if !bytes.Equal(change.ToBytes(), []byte{0xF0}) {
		t.Errorf("Expected 0xF0, got %v", change.ToBytes())
	}
}

func TestApplyPrediction(t *testing.T) {
	prevInput, _ := NewBitVector(8)
	mask, _ := NewBitVector(8)

	prevInput.FromBytes([]byte{0xAA}) // 10101010
	mask.FromBytes([]byte{0xF0})      // 11110000

	// Prediction = AA AND F0 = A0
	prediction := ApplyPrediction(prevInput, mask)

	if !bytes.Equal(prediction.ToBytes(), []byte{0xA0}) {
		t.Errorf("Expected 0xA0, got %v", prediction.ToBytes())
	}
}

func TestComputeResidual(t *testing.T) {
	input, _ := NewBitVector(8)
	prevInput, _ := NewBitVector(8)
	mask, _ := NewBitVector(8)

	input.FromBytes([]byte{0xFF})     // 11111111
	prevInput.FromBytes([]byte{0xAA}) // 10101010
	mask.FromBytes([]byte{0xF0})      // 11110000

	// Prediction = AA AND F0 = A0
	// Residual = FF XOR A0 = 5F
	residual := ComputeResidual(input, prevInput, mask)

	if !bytes.Equal(residual.ToBytes(), []byte{0x5F}) {
		t.Errorf("Expected 0x5F, got %v", residual.ToBytes())
	}
}

func TestApplyReconstruction(t *testing.T) {
	residual, _ := NewBitVector(8)
	prevInput, _ := NewBitVector(8)
	mask, _ := NewBitVector(8)

	residual.FromBytes([]byte{0x5F})  // From previous test
	prevInput.FromBytes([]byte{0xAA}) // 10101010
	mask.FromBytes([]byte{0xF0})      // 11110000

	// Prediction = AA AND F0 = A0
	// Reconstructed = 5F XOR A0 = FF
	reconstructed := ApplyReconstruction(residual, prevInput, mask)

	if !bytes.Equal(reconstructed.ToBytes(), []byte{0xFF}) {
		t.Errorf("Expected 0xFF, got %v", reconstructed.ToBytes())
	}
}

func TestPredictionReconstructionRoundTrip(t *testing.T) {
	// Test that we can reconstruct the original input
	input, _ := NewBitVector(16)
	prevInput, _ := NewBitVector(16)
	mask, _ := NewBitVector(16)

	input.FromBytes([]byte{0x12, 0x34})
	prevInput.FromBytes([]byte{0xAB, 0xCD})
	mask.FromBytes([]byte{0xFF, 0xFF})

	// Compute residual
	residual := ComputeResidual(input, prevInput, mask)

	// Reconstruct
	reconstructed := ApplyReconstruction(residual, prevInput, mask)

	if !reconstructed.Equals(input) {
		t.Errorf("Round-trip failed: expected %v, got %v",
			input.ToBytes(), reconstructed.ToBytes())
	}
}

func TestComputeKt(t *testing.T) {
	mask, _ := NewBitVector(8)
	change, _ := NewBitVector(8)

	mask.FromBytes([]byte{0xAA})   // 10101010
	change.FromBytes([]byte{0x0F}) // 00001111 - changes in lower nibble

	bb := NewBitBuffer()
	err := ComputeKt(bb, mask, change)
	if err != nil {
		t.Errorf("ComputeKt error: %v", err)
	}

	// Kt extracts mask bits at change positions (forward order)
	// Change positions: 4, 5, 6, 7
	// Mask bits at those positions: 1, 0, 1, 0
	// Result: 1010 = 4 bits
	if bb.NumBits() != 4 {
		t.Errorf("Expected 4 bits, got %d", bb.NumBits())
	}
}

func TestMaskUpdateSequence(t *testing.T) {
	// Simulate a sequence of updates
	build, _ := NewBitVector(8)
	mask, _ := NewBitVector(8)
	change, _ := NewBitVector(8)
	prevMask, _ := NewBitVector(8)
	input, _ := NewBitVector(8)
	prevInput, _ := NewBitVector(8)

	// Initial state
	mask.Zero()
	build.Zero()
	prevMask.Zero()
	prevInput.FromBytes([]byte{0x00})

	// t=0: First input
	input.FromBytes([]byte{0xAA})
	UpdateBuild(build, input, prevInput, false, 0)
	UpdateMask(mask, input, prevInput, build, false)
	ComputeChange(change, mask, prevMask, 0)

	// At t=0, mask = changes = input XOR prevInput = AA XOR 00 = AA
	if !bytes.Equal(mask.ToBytes(), []byte{0xAA}) {
		t.Errorf("t=0 mask: expected 0xAA, got %v", mask.ToBytes())
	}

	// Update state for next iteration
	prevMask.CopyFrom(mask)
	prevInput.CopyFrom(input)

	// t=1: Second input - same pattern, no changes
	input.FromBytes([]byte{0xAA})
	UpdateBuild(build, input, prevInput, false, 1)
	UpdateMask(mask, input, prevInput, build, false)
	ComputeChange(change, mask, prevMask, 1)

	// No changes, so change should be zero
	expected, _ := NewBitVector(8)
	expected.Zero()
	if !change.Equals(expected) {
		t.Errorf("t=1 no change: expected 0x00, got %v", change.ToBytes())
	}
}

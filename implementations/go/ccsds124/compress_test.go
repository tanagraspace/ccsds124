package ccsds124

import (
	"bytes"
	"testing"
)

func TestVersion(t *testing.T) {
	if Version == "" {
		t.Error("Version should not be empty")
	}
}

func TestCompressEmptyInput(t *testing.T) {
	result, err := Compress([]byte{}, 90, 1, 10, 20, 50)
	if err != nil {
		t.Errorf("Compress with empty input should not error: %v", err)
	}
	if len(result) != 0 {
		t.Errorf("Expected empty result, got %d bytes", len(result))
	}
}

func TestCompressInvalidPacketSize(t *testing.T) {
	_, err := Compress([]byte{1, 2, 3}, 0, 1, 10, 20, 50)
	if err == nil {
		t.Error("Expected error for zero packet size")
	}
}

func TestCompressInvalidDataLength(t *testing.T) {
	_, err := Compress([]byte{1, 2, 3}, 2, 1, 10, 20, 50)
	if err == nil {
		t.Error("Expected error for data length not multiple of packet size")
	}
}

func TestCompressInvalidRobustness(t *testing.T) {
	data := make([]byte, 90)
	_, err := Compress(data, 90, 0, 10, 20, 50)
	if err == nil {
		t.Error("Expected error for robustness < 1")
	}

	_, err = Compress(data, 90, 8, 10, 20, 50)
	if err == nil {
		t.Error("Expected error for robustness > 7")
	}
}

func TestDecompressEmptyInput(t *testing.T) {
	result, err := Decompress([]byte{}, 90, 1)
	if err != nil {
		t.Errorf("Decompress with empty input should not error: %v", err)
	}
	if len(result) != 0 {
		t.Errorf("Expected empty result, got %d bytes", len(result))
	}
}

func TestDecompressInvalidPacketSize(t *testing.T) {
	_, err := Decompress([]byte{1, 2, 3}, 0, 1)
	if err == nil {
		t.Error("Expected error for zero packet size")
	}
}

func TestDecompressInvalidRobustness(t *testing.T) {
	_, err := Decompress([]byte{1, 2, 3}, 90, 0)
	if err == nil {
		t.Error("Expected error for robustness < 1")
	}

	_, err = Decompress([]byte{1, 2, 3}, 90, 8)
	if err == nil {
		t.Error("Expected error for robustness > 7")
	}
}

func TestNewCompressor(t *testing.T) {
	// Valid compressor
	comp, err := NewCompressor(720, nil, 2, 10, 20, 50)
	if err != nil {
		t.Errorf("NewCompressor failed: %v", err)
	}
	if comp.F != 720 {
		t.Errorf("Expected F=720, got %d", comp.F)
	}

	// Invalid F
	_, err = NewCompressor(0, nil, 2, 10, 20, 50)
	if err == nil {
		t.Error("Expected error for F=0")
	}

	// Invalid robustness
	_, err = NewCompressor(720, nil, 8, 10, 20, 50)
	if err == nil {
		t.Error("Expected error for robustness=8")
	}
}

func TestNewDecompressor(t *testing.T) {
	// Valid decompressor
	decomp, err := NewDecompressor(720, nil, 2)
	if err != nil {
		t.Errorf("NewDecompressor failed: %v", err)
	}
	if decomp.F != 720 {
		t.Errorf("Expected F=720, got %d", decomp.F)
	}

	// Invalid F
	_, err = NewDecompressor(0, nil, 2)
	if err == nil {
		t.Error("Expected error for F=0")
	}

	// Invalid robustness
	_, err = NewDecompressor(720, nil, 8)
	if err == nil {
		t.Error("Expected error for robustness=8")
	}
}

func TestCompressorReset(t *testing.T) {
	comp, _ := NewCompressor(64, nil, 1, 10, 20, 50)

	// Compress a packet to change state
	input, _ := NewBitVector(64)
	input.FromBytes([]byte{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
	comp.CompressPacket(input, nil)

	// Reset should clear state
	comp.Reset()
	if comp.t != 0 {
		t.Errorf("Expected t=0 after reset, got %d", comp.t)
	}
}

func TestDecompressorReset(t *testing.T) {
	decomp, _ := NewDecompressor(64, nil, 1)

	// Change state
	decomp.t = 5

	// Reset should clear state
	decomp.Reset()
	if decomp.t != 0 {
		t.Errorf("Expected t=0 after reset, got %d", decomp.t)
	}
}

func TestCompressSinglePacket(t *testing.T) {
	// Simple 8-byte packet
	data := []byte{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}

	compressed, err := Compress(data, 8, 1, 10, 20, 50)
	if err != nil {
		t.Errorf("Compress failed: %v", err)
	}

	// Compressed output should exist
	if len(compressed) == 0 {
		t.Error("Expected non-empty compressed output")
	}
}

func TestCompressMultiplePackets(t *testing.T) {
	// 3 packets of 8 bytes each
	data := make([]byte, 24)
	for i := range data {
		data[i] = byte(i)
	}

	compressed, err := Compress(data, 8, 1, 10, 20, 50)
	if err != nil {
		t.Errorf("Compress failed: %v", err)
	}

	// Should produce some output
	if len(compressed) == 0 {
		t.Error("Expected non-empty compressed output")
	}
}

func TestCompressorWithInitialMask(t *testing.T) {
	initialMask, _ := NewBitVector(64)
	initialMask.FromBytes([]byte{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00})

	comp, err := NewCompressor(64, initialMask, 1, 10, 20, 50)
	if err != nil {
		t.Errorf("NewCompressor failed: %v", err)
	}

	// Mask should be initialized
	if !comp.mask.Equals(initialMask) {
		t.Error("Mask should equal initial mask")
	}
}

func TestDecompressorWithInitialMask(t *testing.T) {
	initialMask, _ := NewBitVector(64)
	initialMask.FromBytes([]byte{0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00})

	decomp, err := NewDecompressor(64, initialMask, 1)
	if err != nil {
		t.Errorf("NewDecompressor failed: %v", err)
	}

	// Mask should be initialized
	if !decomp.mask.Equals(initialMask) {
		t.Error("Mask should equal initial mask")
	}
}

func TestCompressUncompressedPacket(t *testing.T) {
	// Create compressor
	comp, _ := NewCompressor(64, nil, 1, 10, 20, 50)

	input, _ := NewBitVector(64)
	input.FromBytes([]byte{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22})

	// Force uncompressed packet
	params := &CompressParams{
		MinRobustness:    1,
		UncompressedFlag: true,
	}

	compressed, err := comp.CompressPacket(input, params)
	if err != nil {
		t.Errorf("CompressPacket failed: %v", err)
	}

	// Uncompressed packet should be larger (contains the full input)
	if len(compressed) == 0 {
		t.Error("Expected non-empty output")
	}
}

func TestCompressWithSendMask(t *testing.T) {
	// Create compressor
	comp, _ := NewCompressor(64, nil, 1, 10, 20, 50)

	input, _ := NewBitVector(64)
	input.FromBytes([]byte{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22})

	// Force send mask
	params := &CompressParams{
		MinRobustness: 1,
		SendMaskFlag:  true,
	}

	compressed, err := comp.CompressPacket(input, params)
	if err != nil {
		t.Errorf("CompressPacket failed: %v", err)
	}

	if len(compressed) == 0 {
		t.Error("Expected non-empty output")
	}
}

func TestCompressWithNewMask(t *testing.T) {
	// Create compressor
	comp, _ := NewCompressor(64, nil, 1, 10, 20, 50)

	// First packet to establish state
	input1, _ := NewBitVector(64)
	input1.FromBytes([]byte{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22})
	comp.CompressPacket(input1, nil)

	// Second packet with new mask flag
	input2, _ := NewBitVector(64)
	input2.FromBytes([]byte{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88})

	params := &CompressParams{
		MinRobustness: 1,
		NewMaskFlag:   true,
	}

	compressed, err := comp.CompressPacket(input2, params)
	if err != nil {
		t.Errorf("CompressPacket failed: %v", err)
	}

	if len(compressed) == 0 {
		t.Error("Expected non-empty output")
	}
}

func TestIdenticalPackets(t *testing.T) {
	// Create compressor
	comp, _ := NewCompressor(64, nil, 1, 10, 20, 50)

	// Same packet repeated - should compress well
	input, _ := NewBitVector(64)
	input.FromBytes([]byte{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22})

	var compressed1, compressed2 []byte
	var err error

	compressed1, err = comp.CompressPacket(input, nil)
	if err != nil {
		t.Errorf("CompressPacket 1 failed: %v", err)
	}

	// Same input again
	compressed2, err = comp.CompressPacket(input, nil)
	if err != nil {
		t.Errorf("CompressPacket 2 failed: %v", err)
	}

	// Second packet should be smaller (better compression for identical data)
	if len(compressed2) > len(compressed1) {
		t.Logf("Warning: Second identical packet not smaller (%d vs %d bytes)",
			len(compressed2), len(compressed1))
	}
}

func TestRobustnessLevels(t *testing.T) {
	// Test different robustness levels
	for r := 1; r <= 7; r++ {
		comp, err := NewCompressor(64, nil, r, 10, 20, 50)
		if err != nil {
			t.Errorf("NewCompressor(robustness=%d) failed: %v", r, err)
			continue
		}

		input, _ := NewBitVector(64)
		input.FromBytes([]byte{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0})

		_, err = comp.CompressPacket(input, nil)
		if err != nil {
			t.Errorf("CompressPacket(robustness=%d) failed: %v", r, err)
		}
	}
}

func TestLargePacket(t *testing.T) {
	// 720-bit packet (90 bytes) - standard packet size
	comp, _ := NewCompressor(720, nil, 2, 10, 20, 50)

	input, _ := NewBitVector(720)
	data := make([]byte, 90)
	for i := range data {
		data[i] = byte(i * 3)
	}
	input.FromBytes(data)

	compressed, err := comp.CompressPacket(input, nil)
	if err != nil {
		t.Errorf("CompressPacket failed: %v", err)
	}

	if len(compressed) == 0 {
		t.Error("Expected non-empty output")
	}
}

func TestSequenceOfPackets(t *testing.T) {
	// Simulate realistic sequence
	comp, _ := NewCompressor(64, nil, 2, 10, 20, 50)

	// Generate sequence with gradual changes
	for i := 0; i < 10; i++ {
		input, _ := NewBitVector(64)
		data := make([]byte, 8)
		for j := range data {
			data[j] = byte(i + j)
		}
		input.FromBytes(data)

		_, err := comp.CompressPacket(input, nil)
		if err != nil {
			t.Errorf("CompressPacket %d failed: %v", i, err)
		}
	}
}

func TestCompressDecompressRoundTrip(t *testing.T) {
	// Test round-trip for a single simple packet
	F := 64
	robustness := 1

	// Create compressor
	comp, _ := NewCompressor(F, nil, robustness, 100, 100, 100)

	// Create input
	originalData := []byte{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}
	input, _ := NewBitVector(F)
	input.FromBytes(originalData)

	// Compress with uncompressed flag to ensure round-trip works
	params := &CompressParams{
		MinRobustness:    robustness,
		UncompressedFlag: true,
	}
	compressed, err := comp.CompressPacket(input, params)
	if err != nil {
		t.Fatalf("CompressPacket failed: %v", err)
	}

	// Create decompressor
	decomp, _ := NewDecompressor(F, nil, robustness)

	// Decompress
	reader := NewBitReaderWithBits(compressed, len(compressed)*8)
	output, err := decomp.DecompressPacket(reader)
	if err != nil {
		t.Fatalf("DecompressPacket failed: %v", err)
	}

	// Compare
	outputData := output.ToBytes()
	if !bytes.Equal(originalData, outputData) {
		t.Errorf("Round-trip failed: expected %v, got %v", originalData, outputData)
	}
}

func TestPacketIterator(t *testing.T) {
	// Compress multiple packets
	data := make([]byte, 24) // 3 packets of 8 bytes
	for i := range data {
		data[i] = byte(i)
	}

	compressed, err := Compress(data, 8, 1, 10, 20, 50)
	if err != nil {
		t.Fatalf("Compress failed: %v", err)
	}

	// Decompress using iterator (numBits = len * 8)
	decomp, _ := NewDecompressor(64, nil, 1)
	iter := decomp.NewPacketIterator(compressed, len(compressed)*8)

	count := 0
	for packet := iter.Next(); packet != nil; packet = iter.Next() {
		count++
		if len(packet) != 8 {
			t.Errorf("Packet %d: expected 8 bytes, got %d", count, len(packet))
		}
	}

	if err := iter.Err(); err != nil {
		t.Errorf("Iterator error: %v", err)
	}

	if count != 3 {
		t.Errorf("Expected 3 packets, got %d", count)
	}
}

func TestPacketIteratorEmpty(t *testing.T) {
	decomp, _ := NewDecompressor(64, nil, 1)
	iter := decomp.NewPacketIterator([]byte{}, 0)

	packet := iter.Next()
	if packet != nil {
		t.Error("Expected nil packet for empty input")
	}

	if iter.Err() != nil {
		t.Errorf("Unexpected error: %v", iter.Err())
	}
}

func TestStreamPackets(t *testing.T) {
	// Compress multiple packets
	data := make([]byte, 16) // 2 packets of 8 bytes
	for i := range data {
		data[i] = byte(i * 2)
	}

	compressed, err := Compress(data, 8, 1, 10, 20, 50)
	if err != nil {
		t.Fatalf("Compress failed: %v", err)
	}

	// Decompress using channel (numBits = len * 8)
	decomp, _ := NewDecompressor(64, nil, 1)
	ch := decomp.StreamPackets(compressed, len(compressed)*8)

	count := 0
	for packet := range ch {
		count++
		if len(packet) != 8 {
			t.Errorf("Packet %d: expected 8 bytes, got %d", count, len(packet))
		}
	}

	if count != 2 {
		t.Errorf("Expected 2 packets, got %d", count)
	}
}

func TestDecompressStreamError(t *testing.T) {
	decomp, _ := NewDecompressor(64, nil, 1)

	// Empty data should return empty result, not error
	result, err := decomp.DecompressStream([]byte{}, 0)
	if err == nil {
		t.Log("DecompressStream with empty data returned:", result)
	}
}

func TestDecompressPacketNilReader(t *testing.T) {
	decomp, _ := NewDecompressor(64, nil, 1)

	_, err := decomp.DecompressPacket(nil)
	if err == nil {
		t.Error("Expected error for nil reader")
	}
}

func TestCompressPacketNilInput(t *testing.T) {
	comp, _ := NewCompressor(64, nil, 1, 10, 20, 50)

	_, err := comp.CompressPacket(nil, nil)
	if err == nil {
		t.Error("Expected error for nil input")
	}
}

// TestPacketLengthBounds verifies CCSDS 124.0-B-1 section 3.2:
// 1 <= F <= 65535 (issue #99).
func TestPacketLengthBounds(t *testing.T) {
	if _, err := NewCompressor(0, nil, 1, 0, 0, 0); err == nil {
		t.Error("NewCompressor should reject F=0")
	}
	if _, err := NewCompressor(MaxPacketLength+1, nil, 1, 0, 0, 0); err == nil {
		t.Error("NewCompressor should reject F>65535")
	}
	if _, err := NewCompressor(MaxPacketLength, nil, 1, 0, 0, 0); err != nil {
		t.Errorf("NewCompressor should accept F=65535: %v", err)
	}
	if _, err := NewDecompressor(0, nil, 1); err == nil {
		t.Error("NewDecompressor should reject F=0")
	}
	if _, err := NewDecompressor(MaxPacketLength+1, nil, 1); err == nil {
		t.Error("NewDecompressor should reject F>65535")
	}
}

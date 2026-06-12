package ccsds124

import (
	"bytes"
	"errors"
)

// Decompress decompresses CCSDS 124.0-B-1 compressed data.
//
// Parameters:
//   - data: Compressed input bytes
//   - packetSize: Size of each decompressed packet in bytes
//   - robustness: Robustness parameter R (must match compression)
//
// Returns decompressed data or an error.
func Decompress(data []byte, packetSize, robustness int) ([]byte, error) {
	if len(data) == 0 {
		return []byte{}, nil
	}
	if packetSize <= 0 {
		return nil, errors.New("packet size must be positive")
	}
	if robustness < 1 || robustness > 7 {
		return nil, errors.New("robustness must be between 1 and 7")
	}

	// Convert packet size from bytes to bits
	F := packetSize * 8

	// Create decompressor
	decomp, err := NewDecompressor(F, nil, robustness)
	if err != nil {
		return nil, err
	}

	// Decompress stream
	packets, err := decomp.DecompressStream(data, len(data)*8)
	if err != nil {
		return nil, err
	}

	// Concatenate output
	var output bytes.Buffer
	for _, packet := range packets {
		output.Write(packet)
	}

	return output.Bytes(), nil
}

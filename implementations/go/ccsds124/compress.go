package ccsds124

import (
	"bytes"
	"errors"
)

// Version is the library version.
const Version = "1.0.0"

// ErrNotImplemented is returned when a function is not yet implemented.
var ErrNotImplemented = errors.New("not implemented")

// Compress compresses the input data using CCSDS 124.0-B-1 algorithm.
//
// Parameters:
//   - data: Input bytes to compress (must be multiple of packetSize)
//   - packetSize: Size of each packet in bytes
//   - robustness: Robustness parameter R (1-7)
//   - ptLimit: Period limit for new_mask_flag (pt)
//   - ftLimit: Period limit for send_mask_flag (ft)
//   - rtLimit: Period limit for uncompressed_flag (rt)
//
// Returns compressed data or an error.
func Compress(data []byte, packetSize, robustness, ptLimit, ftLimit, rtLimit int) ([]byte, error) {
	if len(data) == 0 {
		return []byte{}, nil
	}
	if packetSize <= 0 {
		return nil, errors.New("packet size must be positive")
	}
	if len(data)%packetSize != 0 {
		return nil, errors.New("data length must be multiple of packet size")
	}
	if robustness < 1 || robustness > 7 {
		return nil, errors.New("robustness must be between 1 and 7")
	}

	// Convert packet size from bytes to bits
	F := packetSize * 8

	// Create compressor
	comp, err := NewCompressor(F, nil, robustness, ptLimit, ftLimit, rtLimit)
	if err != nil {
		return nil, err
	}

	// Output buffer
	var output bytes.Buffer

	// Number of packets
	numPackets := len(data) / packetSize

	// Compress each packet
	for i := 0; i < numPackets; i++ {
		// Extract packet data
		packetData := data[i*packetSize : (i+1)*packetSize]

		// Create bit vector from packet
		input, err := NewBitVector(F)
		if err != nil {
			return nil, err
		}
		input.FromBytes(packetData)

		// Determine compression parameters (matching C implementation)
		params := &CompressParams{
			MinRobustness: robustness,
		}

		if i == 0 {
			// First packet: fixed init values, counters not checked
			params.SendMaskFlag = true
			params.UncompressedFlag = true
			params.NewMaskFlag = false
		} else {
			// Packets 1+: check and update countdown counters

			// ft counter
			if comp.ftCounter == 1 {
				params.SendMaskFlag = true
				comp.ftCounter = ftLimit
			} else {
				comp.ftCounter--
				params.SendMaskFlag = false
			}

			// pt counter
			if comp.ptCounter == 1 {
				params.NewMaskFlag = true
				comp.ptCounter = ptLimit
			} else {
				comp.ptCounter--
				params.NewMaskFlag = false
			}

			// rt counter
			if comp.rtCounter == 1 {
				params.UncompressedFlag = true
				comp.rtCounter = rtLimit
			} else {
				comp.rtCounter--
				params.UncompressedFlag = false
			}

			// Override for remaining init packets: CCSDS requires first Rt+1 packets
			// to have ft=1, rt=1, pt=0. In 0-indexed: if (i <= Rt)
			if i <= robustness {
				params.SendMaskFlag = true
				params.UncompressedFlag = true
				params.NewMaskFlag = false
			}
		}

		// Compress packet
		compressed, err := comp.CompressPacket(input, params)
		if err != nil {
			return nil, err
		}

		// Append to output
		output.Write(compressed)
	}

	return output.Bytes(), nil
}

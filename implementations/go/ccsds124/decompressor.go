package ccsds124

import (
	"errors"
	"fmt"
)

// Decompressor maintains state for CCSDS 124.0-B-1 decompression.
type Decompressor struct {
	// Configuration (immutable after init)
	F          int // Input vector length in bits
	robustness int // Rt: Base robustness level (0-7)

	// State (updated each cycle)
	mask        *BitVector
	initialMask *BitVector
	prevOutput  *BitVector
	Xt          *BitVector // Positive changes tracker

	// Cycle counter
	t int
}

// NewDecompressor creates a new decompressor.
func NewDecompressor(F int, initialMask *BitVector, robustness int) (*Decompressor, error) {
	if F <= 0 || F > MaxPacketLength {
		return nil, fmt.Errorf("F must be between 1 and %d bits", MaxPacketLength)
	}
	if robustness < 0 || robustness > MaxRobustness {
		return nil, fmt.Errorf("robustness must be between 0 and %d", MaxRobustness)
	}

	decomp := &Decompressor{
		F:          F,
		robustness: robustness,
	}

	// Initialize bit vectors
	var err error
	decomp.mask, err = NewBitVector(F)
	if err != nil {
		return nil, err
	}
	decomp.initialMask, _ = NewBitVector(F)
	decomp.prevOutput, _ = NewBitVector(F)
	decomp.Xt, _ = NewBitVector(F)

	// Set initial mask if provided
	if initialMask != nil {
		decomp.initialMask.CopyFrom(initialMask)
		decomp.mask.CopyFrom(initialMask)
	}

	// Reset state
	decomp.Reset()

	return decomp, nil
}

// Reset resets the decompressor to initial state.
func (decomp *Decompressor) Reset() {
	decomp.t = 0
	decomp.mask.CopyFrom(decomp.initialMask)
	decomp.prevOutput.Zero()
	decomp.Xt.Zero()
}

// DecompressPacket decompresses a single compressed packet.
func (decomp *Decompressor) DecompressPacket(reader *BitReader) (*BitVector, error) {
	if reader == nil {
		return nil, errors.New("reader must not be nil")
	}

	output, _ := NewBitVector(decomp.F)

	// Copy previous output as prediction base
	output.CopyFrom(decomp.prevOutput)

	// Clear positive changes tracker
	decomp.Xt.Zero()

	// ====================================================================
	// Parse ht: Mask change information
	// ht = RLE(Xt) || BIT4(Vt) || et || kt || ct || dt
	// ====================================================================

	// Decode RLE(Xt) - mask changes
	Xt, err := RLEDecode(reader, decomp.F)
	if err != nil {
		return nil, fmt.Errorf("failed to decode RLE(Xt): %w", err)
	}

	// Read BIT4(Vt) - effective robustness
	vtRaw, err := reader.ReadBits(4)
	if err != nil {
		return nil, fmt.Errorf("failed to read Vt: %w", err)
	}
	Vt := int(vtRaw & 0x0F)

	// Process et, kt, ct if Vt > 0 and there are changes
	ct := 0
	changeCount := Xt.HammingWeight()

	if Vt > 0 && changeCount > 0 {
		// Read et
		et, err := reader.ReadBit()
		if err != nil {
			return nil, fmt.Errorf("failed to read et: %w", err)
		}

		if et == 1 {
			// Read kt - determines positive/negative updates
			// kt has one bit per change in Xt
			ktBits := make([]int, 0, changeCount)

			// Read kt bits (forward order)
			for i := 0; i < decomp.F; i++ {
				if Xt.GetBit(i) != 0 {
					bit, err := reader.ReadBit()
					if err != nil {
						return nil, fmt.Errorf("failed to read kt bit: %w", err)
					}
					ktBits = append(ktBits, bit)
				}
			}

			// Apply mask updates based on kt
			ktIdx := 0
			for i := 0; i < decomp.F; i++ {
				if Xt.GetBit(i) != 0 {
					// kt=1 means positive update (mask becomes 0)
					// kt=0 means negative update (mask becomes 1)
					if ktBits[ktIdx] != 0 {
						decomp.mask.SetBit(i, 0)
						decomp.Xt.SetBit(i, 1) // Track positive change
					} else {
						decomp.mask.SetBit(i, 1)
					}
					ktIdx++
				}
			}

			// Read ct
			ctBit, err := reader.ReadBit()
			if err != nil {
				return nil, fmt.Errorf("failed to read ct: %w", err)
			}
			ct = ctBit
		} else {
			// et = 0: all updates are negative (mask bits become 1)
			for i := 0; i < decomp.F; i++ {
				if Xt.GetBit(i) != 0 {
					decomp.mask.SetBit(i, 1)
				}
			}
		}
	} else if Vt == 0 && changeCount > 0 {
		// Vt = 0: toggle mask bits at change positions
		for i := 0; i < decomp.F; i++ {
			if Xt.GetBit(i) != 0 {
				currentVal := decomp.mask.GetBit(i)
				if currentVal == 0 {
					decomp.mask.SetBit(i, 1)
				} else {
					decomp.mask.SetBit(i, 0)
				}
			}
		}
	}
	// else: No changes to apply (changeCount == 0)

	// Read dt
	dt, err := reader.ReadBit()
	if err != nil {
		return nil, fmt.Errorf("failed to read dt: %w", err)
	}

	// ====================================================================
	// Parse qt: Optional full mask
	// ====================================================================

	rt := 0

	// dt=1 means both ft=0 and rt=0 (optimization per CCSDS Eq. 13)
	// dt=0 means we need to read ft and rt from the stream
	if dt == 0 {
		// Read ft flag
		ft, err := reader.ReadBit()
		if err != nil {
			return nil, fmt.Errorf("failed to read ft: %w", err)
		}

		if ft == 1 {
			// Full mask follows: decode RLE(M XOR (M<<))
			maskDiff, err := RLEDecode(reader, decomp.F)
			if err != nil {
				return nil, fmt.Errorf("failed to decode mask: %w", err)
			}

			// Reverse the horizontal XOR to get the actual mask.
			// HXOR encoding: HXOR[i] = M[i] XOR M[i+1], with HXOR[F-1] = M[F-1]
			// Reversal: start from LSB (position F-1) and work towards MSB (position 0)
			// M[F-1] = HXOR[F-1] (just copy)
			// M[i] = HXOR[i] XOR M[i+1] for i < F-1

			// Copy LSB bit directly (position F-1 in bitvector)
			current := maskDiff.GetBit(decomp.F - 1)
			decomp.mask.SetBit(decomp.F-1, current)

			// Process remaining bits from F-2 down to 0
			for i := decomp.F - 1; i > 0; i-- {
				pos := i - 1
				hxorBit := maskDiff.GetBit(pos)
				// M[pos] = HXOR[pos] XOR M[pos+1] = HXOR[pos] XOR current
				current = hxorBit ^ current
				decomp.mask.SetBit(pos, current)
			}
		}

		// Read rt flag
		rtBit, err := reader.ReadBit()
		if err != nil {
			return nil, fmt.Errorf("failed to read rt: %w", err)
		}
		rt = rtBit
	}

	// ====================================================================
	// Parse ut: Unpredictable bits or full input
	// ====================================================================

	if rt == 1 {
		// Full packet follows: COUNT(F) || It
		_, err := CountDecode(reader)
		if err != nil {
			return nil, fmt.Errorf("failed to decode packet length: %w", err)
		}

		// Read full packet
		for i := 0; i < decomp.F; i++ {
			bit, err := reader.ReadBit()
			if err != nil {
				return nil, fmt.Errorf("failed to read input bit %d: %w", i, err)
			}
			output.SetBit(i, bit)
		}
	} else {
		// Compressed: extract unpredictable bits
		var extractionMask *BitVector

		if ct == 1 && Vt > 0 {
			// BE(It, (Xt OR Mt))
			extractionMask = decomp.mask.OR(decomp.Xt)
		} else {
			// BE(It, Mt)
			extractionMask = decomp.mask.Copy()
		}

		// Insert unpredictable bits
		err := BitInsert(reader, output, extractionMask)
		if err != nil {
			return nil, fmt.Errorf("failed to insert bits: %w", err)
		}
	}

	// ====================================================================
	// Update state for next cycle
	// ====================================================================

	decomp.prevOutput.CopyFrom(output)
	decomp.t++

	return output, nil
}

// DecompressStream decompresses multiple packets from a byte stream.
func (decomp *Decompressor) DecompressStream(data []byte, numBits int) ([][]byte, error) {
	if len(data) == 0 {
		return nil, errors.New("input data is empty")
	}

	// Reset decompressor
	decomp.Reset()

	// Initialize bit reader
	reader := NewBitReaderWithBits(data, numBits)

	// Output packet size in bytes
	packetBytes := (decomp.F + 7) / 8
	var outputs [][]byte

	// Decompress packets until input exhausted
	for reader.Remaining() > 0 {
		output, err := decomp.DecompressPacket(reader)
		if err != nil {
			return outputs, err
		}

		// Convert to bytes
		outputBytes := output.ToBytes()
		if len(outputBytes) < packetBytes {
			// Pad if necessary
			padded := make([]byte, packetBytes)
			copy(padded, outputBytes)
			outputBytes = padded
		}
		outputs = append(outputs, outputBytes[:packetBytes])

		// Align to byte boundary for next packet
		reader.AlignByte()
	}

	return outputs, nil
}

// PacketIterator provides streaming decompression with an iterator pattern.
type PacketIterator struct {
	decomp      *Decompressor
	reader      *BitReader
	packetBytes int
	err         error
}

// NewPacketIterator creates a streaming packet iterator.
func (decomp *Decompressor) NewPacketIterator(data []byte, numBits int) *PacketIterator {
	decomp.Reset()
	return &PacketIterator{
		decomp:      decomp,
		reader:      NewBitReaderWithBits(data, numBits),
		packetBytes: (decomp.F + 7) / 8,
	}
}

// Next returns the next decompressed packet, or nil if done/error.
func (it *PacketIterator) Next() []byte {
	if it.err != nil || it.reader.Remaining() <= 0 {
		return nil
	}

	output, err := it.decomp.DecompressPacket(it.reader)
	if err != nil {
		it.err = err
		return nil
	}

	// Convert to bytes
	outputBytes := output.ToBytes()
	if len(outputBytes) < it.packetBytes {
		padded := make([]byte, it.packetBytes)
		copy(padded, outputBytes)
		outputBytes = padded
	}

	it.reader.AlignByte()
	return outputBytes[:it.packetBytes]
}

// Err returns any error encountered during iteration.
func (it *PacketIterator) Err() error {
	return it.err
}

// StreamPackets returns a channel that yields decompressed packets.
// The channel is closed when all packets are processed or on error.
func (decomp *Decompressor) StreamPackets(data []byte, numBits int) <-chan []byte {
	ch := make(chan []byte, 64) // Buffer some packets

	go func() {
		defer close(ch)

		decomp.Reset()
		reader := NewBitReaderWithBits(data, numBits)
		packetBytes := (decomp.F + 7) / 8

		for reader.Remaining() > 0 {
			output, err := decomp.DecompressPacket(reader)
			if err != nil {
				return // Stop on error
			}

			outputBytes := output.ToBytes()
			if len(outputBytes) < packetBytes {
				padded := make([]byte, packetBytes)
				copy(padded, outputBytes)
				outputBytes = padded
			}

			ch <- outputBytes[:packetBytes]
			reader.AlignByte()
		}
	}()

	return ch
}

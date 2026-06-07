package pocketplus

import (
	"errors"
	"fmt"
)

// Constants
const (
	MaxHistory    = 16 // History depth for change vectors
	MaxVtHistory  = 16 // History size for Vt calculation
	MaxRobustness = 7  // Maximum robustness level
)

// CompressParams holds compression parameters for a single packet.
type CompressParams struct {
	MinRobustness    int  // Rt: Minimum robustness level (0-7)
	NewMaskFlag      bool // pt: Update mask from build vector
	SendMaskFlag     bool // ft: Include mask in output
	UncompressedFlag bool // rt: Send uncompressed
}

// Compressor maintains state for POCKET+ compression.
type Compressor struct {
	// Configuration (immutable after init)
	F          int // Input vector length in bits
	robustness int // Rt: Base robustness level (0-7)

	// Period limits for automatic parameter management
	ptLimit int
	ftLimit int
	rtLimit int

	// State (updated each cycle)
	mask        *BitVector
	prevMask    *BitVector
	build       *BitVector
	prevInput   *BitVector
	initialMask *BitVector

	// Change history (circular buffer)
	changeHistory [MaxHistory]*BitVector
	historyIndex  int

	// Flag history for ct calculation
	newMaskFlagHistory [MaxVtHistory]int
	flagHistoryIndex   int

	// Cycle counter
	t int

	// Countdown counters for automatic mode
	ptCounter int
	ftCounter int
	rtCounter int

	// Pre-allocated working buffers (avoid per-packet allocations)
	workChange      *BitVector // For change computation
	workXt          *BitVector // For robustness window
	workCombined    *BitVector // For combining changes
	workInvMask     *BitVector // For inverted mask
	workExtractMask *BitVector // For extraction mask
	workMaskShifted *BitVector // For mask shift
	workMaskDiff    *BitVector // For mask XOR
	workChanges     *BitVector // For input XOR prevInput
	workOutput      *BitBuffer // For output buffer
}

// NewCompressor creates a new compressor.
func NewCompressor(F int, initialMask *BitVector, robustness, ptLimit, ftLimit, rtLimit int) (*Compressor, error) {
	if F <= 0 {
		return nil, errors.New("F must be positive")
	}
	if robustness < 0 || robustness > MaxRobustness {
		return nil, fmt.Errorf("robustness must be between 0 and %d", MaxRobustness)
	}

	comp := &Compressor{
		F:          F,
		robustness: robustness,
		ptLimit:    ptLimit,
		ftLimit:    ftLimit,
		rtLimit:    rtLimit,
	}

	// Initialize bit vectors
	var err error
	comp.mask, err = NewBitVector(F)
	if err != nil {
		return nil, err
	}
	comp.prevMask, _ = NewBitVector(F)
	comp.build, _ = NewBitVector(F)
	comp.prevInput, _ = NewBitVector(F)
	comp.initialMask, _ = NewBitVector(F)

	// Initialize change history
	for i := 0; i < MaxHistory; i++ {
		comp.changeHistory[i], _ = NewBitVector(F)
	}

	// Initialize working buffers
	comp.workChange, _ = NewBitVector(F)
	comp.workXt, _ = NewBitVector(F)
	comp.workCombined, _ = NewBitVector(F)
	comp.workInvMask, _ = NewBitVector(F)
	comp.workExtractMask, _ = NewBitVector(F)
	comp.workMaskShifted, _ = NewBitVector(F)
	comp.workMaskDiff, _ = NewBitVector(F)
	comp.workChanges, _ = NewBitVector(F)
	comp.workOutput = NewBitBuffer()

	// Set initial mask if provided
	if initialMask != nil {
		comp.initialMask.CopyFrom(initialMask)
		comp.mask.CopyFrom(initialMask)
	}

	// Reset state
	comp.Reset()

	return comp, nil
}

// Reset resets the compressor to initial state.
func (comp *Compressor) Reset() {
	comp.t = 0
	comp.historyIndex = 0

	// Reset mask to initial
	comp.mask.CopyFrom(comp.initialMask)
	comp.prevMask.Zero()

	// Clear build and prevInput
	comp.build.Zero()
	comp.prevInput.Zero()

	// Clear change history
	for i := 0; i < MaxHistory; i++ {
		comp.changeHistory[i].Zero()
	}

	// Clear flag history
	for i := 0; i < MaxVtHistory; i++ {
		comp.newMaskFlagHistory[i] = 0
	}
	comp.flagHistoryIndex = 0

	// Reset countdown counters
	comp.ptCounter = comp.ptLimit
	comp.ftCounter = comp.ftLimit
	comp.rtCounter = comp.rtLimit
}

// CompressPacket compresses a single input packet.
func (comp *Compressor) CompressPacket(input *BitVector, params *CompressParams) ([]byte, error) {
	if input == nil || input.length != comp.F {
		return nil, errors.New("input must be non-nil and match F length")
	}

	// Use default params if none provided
	if params == nil {
		params = &CompressParams{MinRobustness: comp.robustness}
	}

	// Reuse pre-allocated output buffer
	comp.workOutput.Clear()
	output := comp.workOutput

	// ================================================================
	// STEP 1: Update Mask and Build Vectors (CCSDS Section 4)
	// ================================================================

	// Save previous mask (reuse prevMask field)
	comp.prevMask.CopyFrom(comp.mask)
	prevMask := comp.prevMask

	// Save previous build into workCombined temporarily
	comp.workCombined.CopyFrom(comp.build)
	prevBuild := comp.workCombined

	// Update build vector (Equation 6)
	if comp.t > 0 {
		updateBuildInternal(comp.build, input, comp.prevInput, comp.workChanges, params.NewMaskFlag, comp.t)
	}

	// Update mask vector (Equation 7)
	if comp.t > 0 {
		updateMaskInternal(comp.mask, input, comp.prevInput, prevBuild, comp.workChanges, params.NewMaskFlag)
	}

	// Compute change vector (Equation 8) - reuse workChange
	change := comp.workChange
	computeChangeInternal(change, comp.mask, prevMask, comp.t)

	// Store change in history (circular buffer)
	comp.changeHistory[comp.historyIndex].CopyFrom(change)

	// ================================================================
	// STEP 2: Encode Output Packet (CCSDS Section 5.3)
	// ot = ht || qt || ut
	// ================================================================

	// Calculate Xt (robustness window) - reuse workXt
	Xt := comp.computeRobustnessWindowInto(change, comp.workXt)

	// Calculate Vt (effective robustness)
	Vt := comp.computeEffectiveRobustness(change)

	// Calculate dt flag
	var dt int
	if !params.SendMaskFlag && !params.UncompressedFlag {
		dt = 1
	}

	// ================================================================
	// Component ht: Mask change information
	// ht = RLE(Xt) || BIT4(Vt) || et || kt || ct || dt
	// ================================================================

	// 1. RLE(Xt) - Run-length encode the robustness window
	RLEEncode(output, Xt)

	// 2. BIT4(Vt) - 4-bit effective robustness level
	output.AppendValue(uint64(Vt), 4)

	// 3. et, kt, ct - Only if Vt > 0 and there are mask changes
	if Vt > 0 && Xt.HammingWeight() > 0 {
		// Calculate et
		et := hasPositiveUpdates(Xt, comp.mask)
		output.AppendBit(et)

		if et != 0 {
			// kt - Output '1' for positive updates (mask=0), '0' for negative
			// Extract INVERTED mask values in forward order - reuse workInvMask
			invertedMask := comp.workInvMask
			for j := 0; j < comp.mask.length; j++ {
				maskBit := comp.mask.GetBit(j)
				if maskBit == 0 {
					invertedMask.SetBit(j, 1)
				} else {
					invertedMask.SetBit(j, 0)
				}
			}
			BitExtractForward(output, invertedMask, Xt)

			// Calculate and encode ct
			ct := comp.computeCtFlag(Vt, params.NewMaskFlag)
			output.AppendBit(ct)
		}
	}

	// 4. dt - Flag indicating if both ft and rt are zero
	output.AppendBit(dt)

	// ================================================================
	// Component qt: Optional full mask
	// qt = empty if dt=1, '1' || RLE(<(Mt XOR (Mt<<))>) if ft=1, '0' otherwise
	// ================================================================

	if dt == 0 { // Only if dt = 0
		if params.SendMaskFlag {
			output.AppendBit(1) // Flag: mask follows

			// Encode mask as RLE(M XOR (M<<)) - reuse working buffers
			leftShiftInto(comp.workMaskShifted, comp.mask)
			comp.workMaskDiff.XORInto(comp.mask, comp.workMaskShifted)
			RLEEncode(output, comp.workMaskDiff)
		} else {
			output.AppendBit(0) // Flag: no mask
		}
	}

	// ================================================================
	// Component ut: Unpredictable bits or full input
	// ================================================================

	if params.UncompressedFlag {
		// '1' || COUNT(F) || It
		output.AppendBit(1) // Flag: full input follows
		CountEncode(output, comp.F)
		output.AppendBitVector(input)
	} else {
		if dt == 0 {
			// '0' || BE(...)
			output.AppendBit(0) // Flag: compressed
		}

		// Determine extraction mask based on ct
		ct := comp.computeCtFlag(Vt, params.NewMaskFlag)

		if ct != 0 && Vt > 0 {
			// BE(It, (Xt OR Mt)) - extract bits where mask OR changes are set
			comp.workExtractMask.ORInto(comp.mask, Xt)
			BitExtract(output, input, comp.workExtractMask)
		} else {
			// BE(It, Mt) - extract only unpredictable bits
			BitExtract(output, input, comp.mask)
		}
	}

	// ================================================================
	// STEP 3: Update State for Next Cycle
	// ================================================================

	// Save current input and mask as previous for next iteration
	comp.prevInput.CopyFrom(input)
	comp.prevMask.CopyFrom(comp.mask)

	// Track new_mask_flag for ct calculation
	if params.NewMaskFlag {
		comp.newMaskFlagHistory[comp.flagHistoryIndex] = 1
	} else {
		comp.newMaskFlagHistory[comp.flagHistoryIndex] = 0
	}
	comp.flagHistoryIndex = (comp.flagHistoryIndex + 1) % MaxVtHistory

	// Advance time
	comp.t++

	// Advance history index (circular buffer)
	comp.historyIndex = (comp.historyIndex + 1) % MaxHistory

	return output.ToBytes(), nil
}

// computeRobustnessWindowInto computes Xt = OR of recent change vectors into dst.
func (comp *Compressor) computeRobustnessWindowInto(currentChange *BitVector, dst *BitVector) *BitVector {
	if comp.robustness == 0 || comp.t == 0 {
		// Xt = Dt (no reversal - RLE processes LSB to MSB directly)
		dst.CopyFrom(currentChange)
	} else {
		// Start with current change
		dst.CopyFrom(currentChange)

		// Determine how many historical changes to include
		numChanges := comp.t
		if comp.robustness < numChanges {
			numChanges = comp.robustness
		}

		// OR with historical changes (going backwards from current)
		for i := 1; i <= numChanges; i++ {
			// Calculate index of change from i iterations ago
			histIdx := (comp.historyIndex + MaxHistory - i) % MaxHistory
			// OR in place: dst = dst OR changeHistory[histIdx]
			for w := 0; w < dst.numWords; w++ {
				dst.data[w] |= comp.changeHistory[histIdx].data[w]
			}
		}
	}

	return dst
}

// leftShiftInto computes left shift of src into dst.
func leftShiftInto(dst, src *BitVector) {
	// Word-level left shift (big-endian: MSB in high bits of first word)
	var carry uint32
	for i := src.numWords - 1; i >= 0; i-- {
		word := src.data[i]
		dst.data[i] = ((word << 1) | carry) & 0xFFFFFFFF
		carry = (word >> 31) & 1
	}
}

// computeEffectiveRobustness computes Vt = Rt + Ct.
func (comp *Compressor) computeEffectiveRobustness(currentChange *BitVector) int {
	_ = currentChange // Unused - Ct is computed from history

	Rt := comp.robustness
	Vt := Rt

	// For t > Rt, compute Ct
	if comp.t > Rt {
		// Count backwards through history starting from Rt+1 positions back
		Ct := 0

		maxI := 15
		if comp.t < maxI {
			maxI = comp.t
		}

		for i := Rt + 1; i <= maxI; i++ {
			histIdx := (comp.historyIndex + MaxHistory - i) % MaxHistory
			if comp.changeHistory[histIdx].HammingWeight() > 0 {
				break // Found a change, stop counting
			}
			Ct++
			if Ct >= 15-Rt {
				break // Cap at maximum Ct value
			}
		}

		Vt = Rt + Ct
		if Vt > 15 {
			Vt = 15 // Cap at 15 (4 bits)
		}
	}

	return Vt
}

// computeCtFlag computes ct flag for multiple mask updates.
func (comp *Compressor) computeCtFlag(Vt int, currentNewMaskFlag bool) int {
	if Vt == 0 {
		return 0
	}

	// Count how many times new_mask_flag was set
	count := 0

	// Include current packet's flag
	if currentNewMaskFlag {
		count++
	}

	// Check history for Vt previous entries
	iterationsToCheck := Vt
	if comp.t < iterationsToCheck {
		iterationsToCheck = comp.t
	}

	for i := 0; i < iterationsToCheck; i++ {
		// Calculate history index going backwards from previous
		histIdx := (comp.flagHistoryIndex + MaxVtHistory - 1 - i) % MaxVtHistory
		if comp.newMaskFlagHistory[histIdx] != 0 {
			count++
		}
	}

	if count >= 2 {
		return 1
	}
	return 0
}

// hasPositiveUpdates checks for positive mask updates (et flag).
func hasPositiveUpdates(Xt, mask *BitVector) int {
	// et = 1 if any changed bits (in Xt) are predictable (mask bit = 0)
	for i := 0; i < Xt.length; i++ {
		bitChanged := Xt.GetBit(i)
		bitPredictable := mask.GetBit(i) == 0

		if bitChanged != 0 && bitPredictable {
			return 1 // Found a positive update
		}
	}
	return 0
}

// Internal helper functions that modify vectors in place

func updateBuildInternal(build, inputVec, prevInput, workChanges *BitVector, newMaskFlag bool, t int) {
	if t == 0 || newMaskFlag {
		build.Zero()
	} else {
		// Bt = (It XOR It-1) OR Bt-1
		workChanges.XORInto(inputVec, prevInput)
		build.ORInto(workChanges, build)
	}
}

func updateMaskInternal(mask, inputVec, prevInput, buildPrev, workChanges *BitVector, newMaskFlag bool) {
	// Calculate changes: It XOR It-1 (into workChanges)
	workChanges.XORInto(inputVec, prevInput)

	if newMaskFlag {
		// Mt = changes OR Bt-1
		mask.ORInto(workChanges, buildPrev)
	} else {
		// Mt = changes OR Mt-1 (in place)
		for w := 0; w < mask.numWords; w++ {
			mask.data[w] = workChanges.data[w] | mask.data[w]
		}
	}
}

func computeChangeInternal(change, mask, prevMask *BitVector, t int) {
	if t == 0 {
		// CCSDS Eq. 8: D0 = 0 — both encoder and decoder start from the
		// same user-specified M0, so there is no change to communicate
		change.Zero()
	} else {
		// Dt = Mt XOR Mt-1
		change.XORInto(mask, prevMask)
	}
}

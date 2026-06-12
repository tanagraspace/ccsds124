package ccsds124

// UpdateBuild updates the build vector (CCSDS Equation 6).
//
// Build vector accumulates bits that have changed over time.
// When newMaskFlag is set, the build is used to replace the mask
// and is then reset to zero.
//
// Equation:
//   - Bt = (It XOR It-1) OR Bt-1  (if t > 0 and newMaskFlag = 0)
//   - Bt = 0                      (if t = 0 or newMaskFlag = 1)
func UpdateBuild(build, inputVec, prevInput *BitVector, newMaskFlag bool, t int) {
	if t == 0 || newMaskFlag {
		// Reset build to zero
		build.Zero()
	} else {
		// Bt = (It XOR It-1) OR Bt-1
		// Calculate changes
		changes := inputVec.XOR(prevInput)

		// Update build: build = changes OR build
		build.ORInto(changes, build)
	}
}

// UpdateMask updates the mask vector (CCSDS Equation 7).
//
// The mask tracks which bits are unpredictable.
// When newMaskFlag is set, the mask is replaced with the build vector.
//
// Equation:
//   - Mt = (It XOR It-1) OR Mt-1  (if newMaskFlag = 0)
//   - Mt = (It XOR It-1) OR Bt-1  (if newMaskFlag = 1)
func UpdateMask(mask, inputVec, prevInput, buildPrev *BitVector, newMaskFlag bool) {
	// Calculate changes: It XOR It-1
	changes := inputVec.XOR(prevInput)

	if newMaskFlag {
		// Mt = changes OR Bt-1
		mask.ORInto(changes, buildPrev)
	} else {
		// Mt = changes OR Mt-1
		result := changes.OR(mask)
		mask.CopyFrom(result)
	}
}

// ComputeChange computes the change vector (CCSDS Equation 8).
//
// The change vector tracks which mask bits changed between time steps.
// This is used in encoding to communicate mask updates.
//
// Equation:
//   - Dt = Mt XOR Mt-1  (if t > 0)
//   - Dt = 0            (if t = 0, CCSDS Eq. 8: no change at initialization)
func ComputeChange(change, mask, prevMask *BitVector, t int) {
	if t == 0 {
		// CCSDS Eq. 8: D0 = 0 — both encoder and decoder start from the
		// same user-specified M0, so there is no change to communicate
		change.Zero()
	} else {
		// Dt = Mt XOR Mt-1
		change.XORInto(mask, prevMask)
	}
}

// ComputeKt computes the kt component for encoding.
//
// kt = BE(Mt, Dt) - bits of mask at positions where change occurred
// This tells the decompressor which mask values to expect.
func ComputeKt(bb *BitBuffer, mask, change *BitVector) error {
	return BitExtractForward(bb, mask, change)
}

// ApplyPrediction applies prediction to get predicted value.
//
// Equation:
//   - P(It) = It-1 AND Mt (prediction based on mask)
//   - If mask bit is 1, predict same as previous
//   - If mask bit is 0, predict 0
func ApplyPrediction(prevInput, mask *BitVector) *BitVector {
	return prevInput.AND(mask)
}

// ComputeResidual computes residual (difference from prediction).
//
// Equation:
//   - Rt = It XOR P(It)
//   - Rt = It XOR (It-1 AND Mt)
func ComputeResidual(input, prevInput, mask *BitVector) *BitVector {
	prediction := ApplyPrediction(prevInput, mask)
	return input.XOR(prediction)
}

// ApplyReconstruction reconstructs input from residual and prediction.
//
// Equation:
//   - It = Rt XOR P(It)
//   - It = Rt XOR (It-1 AND Mt)
func ApplyReconstruction(residual, prevInput, mask *BitVector) *BitVector {
	prediction := ApplyPrediction(prevInput, mask)
	return residual.XOR(prediction)
}

/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

/**
 * Compressor implementing CCSDS 124.0-B-1 Section 5.3.
 *
 * <p>Output packet format: oₜ = hₜ ∥ qₜ ∥ uₜ
 *
 * <ul>
 *   <li>hₜ = RLE(Xₜ) ∥ BIT₄(Vₜ) ∥ eₜ ∥ kₜ ∥ cₜ ∥ ḋₜ
 *   <li>qₜ = mask transmission (if needed)
 *   <li>uₜ = unpredictable bits or full input
 * </ul>
 */
public final class Compressor {

  private static final int MAX_HISTORY = 16;
  private static final int MAX_VT_HISTORY = 16;

  // Configuration
  private final int length;
  private final int robustness;
  private final BitVector initialMask;
  private final int ptLimit;
  private final int ftLimit;
  private final int rtLimit;

  // State
  private BitVector mask;
  private BitVector prevMask;
  private BitVector build;
  private BitVector prevInput;
  private final BitVector[] changeHistory;
  private final boolean[] flagHistory;
  private int historyIndex;
  private int flagHistoryIndex;
  private int t;

  // Countdown counters (match C reference)
  private int ptCounter;
  private int ftCounter;
  private int rtCounter;

  // Pre-allocated work buffers (avoid per-packet allocation)
  private final BitVector workPrevBuild;
  private final BitVector workChange;
  private final BitVector workXt;
  private final BitVector workShifted;
  private final BitVector workDiff;
  private final BitVector workExtractMask;

  /**
   * Creates a new Compressor.
   *
   * @param length packet length in bits
   * @param initialMask initial mask vector (null for all zeros)
   * @param robustness robustness parameter R (0-7)
   * @param ptLimit Pt parameter limit
   * @param ftLimit Ft parameter limit
   * @param rtLimit Rt parameter limit
   */
  public Compressor(
      int length, BitVector initialMask, int robustness, int ptLimit, int ftLimit, int rtLimit) {
    if (length <= 0 || length > Ccsds124.MAX_PACKET_LENGTH) {
      throw new IllegalArgumentException("Invalid packet length: " + length);
    }
    if (robustness < 0 || robustness > Ccsds124.MAX_ROBUSTNESS) {
      throw new IllegalArgumentException("Invalid robustness: " + robustness);
    }

    this.length = length;
    this.robustness = robustness;
    this.initialMask = initialMask != null ? new BitVector(initialMask) : new BitVector(length);
    this.ptLimit = ptLimit;
    this.ftLimit = ftLimit;
    this.rtLimit = rtLimit;

    // Initialize state
    this.mask = new BitVector(length);
    this.prevMask = new BitVector(length);
    this.build = new BitVector(length);
    this.prevInput = new BitVector(length);
    this.changeHistory = new BitVector[MAX_HISTORY];
    this.flagHistory = new boolean[MAX_VT_HISTORY];
    for (int i = 0; i < MAX_HISTORY; i++) {
      this.changeHistory[i] = new BitVector(length);
    }

    // Pre-allocate work buffers
    this.workPrevBuild = new BitVector(length);
    this.workChange = new BitVector(length);
    this.workXt = new BitVector(length);
    this.workShifted = new BitVector(length);
    this.workDiff = new BitVector(length);
    this.workExtractMask = new BitVector(length);

    reset();
  }

  /** Resets the compressor to initial state. */
  public void reset() {
    t = 0;
    historyIndex = 0;
    flagHistoryIndex = 0;

    // Reset mask to initial
    copyBitVector(mask, initialMask);
    prevMask.zero();
    build.zero();
    prevInput.zero();

    // Clear history
    for (int i = 0; i < MAX_HISTORY; i++) {
      changeHistory[i].zero();
    }
    for (int i = 0; i < MAX_VT_HISTORY; i++) {
      flagHistory[i] = false;
    }

    // Reset counters (match C reference: start at limit)
    ptCounter = ptLimit;
    ftCounter = ftLimit;
    rtCounter = rtLimit;
  }

  /**
   * Compresses a single packet using automatic parameter management.
   *
   * @param input the input packet as a BitVector
   * @return compressed output as a BitBuffer
   */
  public BitBuffer compressPacket(BitVector input) {
    // Compute parameters automatically
    boolean sendMaskFlag;
    boolean uncompressedFlag;
    boolean newMaskFlag;

    if (t == 0) {
      // First packet: fixed init values
      sendMaskFlag = true;
      uncompressedFlag = true;
      newMaskFlag = false;
    } else {
      // Check and update countdown counters (C reference style)
      if (ftCounter == 1) {
        sendMaskFlag = true;
        ftCounter = ftLimit;
      } else {
        ftCounter--;
        sendMaskFlag = false;
      }

      if (ptCounter == 1) {
        newMaskFlag = true;
        ptCounter = ptLimit;
      } else {
        ptCounter--;
        newMaskFlag = false;
      }

      if (rtCounter == 1) {
        uncompressedFlag = true;
        rtCounter = rtLimit;
      } else {
        rtCounter--;
        uncompressedFlag = false;
      }

      // CCSDS: first R+1 packets require ft=1, rt=1, pt=0
      if (t <= robustness) {
        sendMaskFlag = true;
        uncompressedFlag = true;
        newMaskFlag = false;
      }
    }

    return compressPacketWithParams(input, newMaskFlag, sendMaskFlag, uncompressedFlag);
  }

  /**
   * Compresses a packet with explicit parameters.
   *
   * @param input input packet
   * @param newMaskFlag whether to start new mask period
   * @param sendMaskFlag whether to send full mask
   * @param uncompressedFlag whether to send uncompressed
   * @return compressed output
   */
  public BitBuffer compressPacketWithParams(
      BitVector input, boolean newMaskFlag, boolean sendMaskFlag, boolean uncompressedFlag) {

    BitBuffer output = new BitBuffer();

    // Save previous mask and build before update (use pre-allocated buffers)
    prevMask.copyFrom(mask);
    workPrevBuild.copyFrom(build);

    // Update build vector (Equation 6) if t > 0
    if (t > 0) {
      MaskOperations.updateBuildInPlace(build, input, prevInput, newMaskFlag);
    }

    // Update mask vector (Equation 7) if t > 0
    if (t > 0) {
      MaskOperations.updateMaskInPlace(mask, input, prevInput, workPrevBuild, newMaskFlag);
    }

    // Compute change vector (Equation 8) into pre-allocated buffer
    MaskOperations.computeChangeInto(workChange, mask, prevMask, t);

    // Store change in history
    changeHistory[historyIndex].copyFrom(workChange);

    // === Encode output packet ===
    // oₜ = hₜ ∥ qₜ ∥ uₜ

    // Compute Xₜ (robustness window) into pre-allocated buffer
    computeRobustnessWindowInto(workXt, workChange);

    // Compute Vₜ (effective robustness)
    int vt = computeEffectiveRobustness();

    // Compute ḋₜ flag
    int dt = (!sendMaskFlag && !uncompressedFlag) ? 1 : 0;

    // === Component hₜ ===
    // hₜ = RLE(Xₜ) ∥ BIT₄(Vₜ) ∥ eₜ ∥ kₜ ∥ cₜ ∥ ḋₜ

    // 1. RLE(Xₜ)
    Encoder.rleEncode(output, workXt);

    // 2. BIT₄(Vₜ) - use bulk append
    output.appendBits(vt, 4);

    // 3. eₜ, kₜ, cₜ (only if Vt > 0 and there are changes)
    if (vt > 0 && workXt.hammingWeight() > 0) {
      // eₜ: positive updates exist?
      boolean et = hasPositiveUpdates(workXt, mask);
      output.appendBit(et ? 1 : 0);

      if (et) {
        // kₜ: extract inverted mask at change positions
        // Use workExtractMask as temp for inverted mask
        workExtractMask.copyFrom(mask);
        invertBitVector(workExtractMask);
        Encoder.bitExtractForward(output, workExtractMask, workXt);

        // cₜ: new_mask_flag was set 2+ times in last Vt+1 iterations
        int ct = computeCtFlag(vt, newMaskFlag);
        output.appendBit(ct);
      }
    }

    // 4. ḋₜ
    output.appendBit(dt);

    // === Component qₜ ===
    if (dt == 0) {
      if (sendMaskFlag) {
        output.appendBit(1); // Flag: mask follows
        // RLE(M XOR (M<<)) using pre-allocated buffers
        workShifted.copyFrom(mask);
        workShifted.leftShiftInPlace();
        workDiff.copyFrom(mask);
        workDiff.xorInPlace(workShifted);
        Encoder.rleEncode(output, workDiff);
      } else {
        output.appendBit(0); // Flag: no mask
      }
    }

    // === Component uₜ ===
    if (uncompressedFlag) {
      // '1' ∥ COUNT(F) ∥ Iₜ
      output.appendBit(1);
      Encoder.countEncode(output, length);
      output.appendBitVector(input, length);
    } else {
      if (dt == 0) {
        output.appendBit(0); // Flag: compressed
      }

      // Determine extraction mask using pre-allocated buffer
      int ct = computeCtFlag(vt, newMaskFlag);
      if (ct != 0 && vt > 0) {
        workExtractMask.copyFrom(mask);
        workExtractMask.orInPlace(workXt);
        Encoder.bitExtract(output, input, workExtractMask);
      } else {
        Encoder.bitExtract(output, input, mask);
      }
    }

    // === Update state ===
    copyBitVector(prevInput, input);

    // Track new_mask_flag
    flagHistory[flagHistoryIndex] = newMaskFlag;
    flagHistoryIndex = (flagHistoryIndex + 1) % MAX_VT_HISTORY;

    // Advance time and history
    t++;
    historyIndex = (historyIndex + 1) % MAX_HISTORY;

    return output;
  }

  private void computeRobustnessWindowInto(BitVector xt, BitVector currentChange) {
    // Start with current change
    xt.copyFrom(currentChange);

    if (robustness > 0 && t > 0) {
      // OR with historical changes using in-place operation
      int numChanges = Math.min(t, robustness);
      for (int i = 1; i <= numChanges; i++) {
        int histIdx = (historyIndex + MAX_HISTORY - i) % MAX_HISTORY;
        xt.orInPlace(changeHistory[histIdx]);
      }
    }
  }

  private void invertBitVector(BitVector bv) {
    // In-place bit inversion
    for (int i = 0; i < bv.numWords(); i++) {
      bv.setWord(i, ~bv.getWord(i));
    }
  }

  private int computeEffectiveRobustness() {
    // Vₜ = Rₜ + Cₜ
    int vt = robustness;

    if (t > robustness) {
      // Count consecutive iterations with no mask changes
      int ct = 0;
      for (int i = robustness + 1; i <= 15 && i <= t; i++) {
        int histIdx = (historyIndex + MAX_HISTORY - i) % MAX_HISTORY;
        if (changeHistory[histIdx].hammingWeight() > 0) {
          break;
        }
        ct++;
        if (ct >= 15 - robustness) {
          break;
        }
      }
      vt = robustness + ct;
      if (vt > 15) {
        vt = 15;
      }
    }

    return vt;
  }

  private boolean hasPositiveUpdates(BitVector xt, BitVector maskVec) {
    // eₜ = 1 if any changed bits have mask=0
    int numWords = Math.min(xt.numWords(), maskVec.numWords());
    for (int i = 0; i < numWords; i++) {
      if ((xt.getWord(i) & ~maskVec.getWord(i)) != 0) {
        return true;
      }
    }
    return false;
  }

  private int computeCtFlag(int vt, boolean currentNewMaskFlag) {
    // cₜ = 1 if new_mask_flag was set 2+ times in last Vt+1 iterations
    if (vt == 0) {
      return 0;
    }

    int count = currentNewMaskFlag ? 1 : 0;
    int itersToCheck = Math.min(vt, t);

    for (int i = 0; i < itersToCheck; i++) {
      int histIdx = (flagHistoryIndex + MAX_VT_HISTORY - 1 - i) % MAX_VT_HISTORY;
      if (flagHistory[histIdx]) {
        count++;
      }
    }

    return count >= 2 ? 1 : 0;
  }

  private void copyBitVector(BitVector dest, BitVector src) {
    dest.copyFrom(src);
  }

  /**
   * Returns the current time index.
   *
   * @return time index t
   */
  public int getTimeIndex() {
    return t;
  }
}

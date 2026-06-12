/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

/**
 * CCSDS 124.0-B-1 decompressor state machine.
 *
 * <p>This class implements the CCSDS 124.0-B-1 decompression algorithm as defined in CCSDS
 * 124.0-B-1. It maintains the decompression state across multiple packets and reconstructs the
 * original data.
 *
 * <p>Packet format: oₜ = hₜ ∥ qₜ ∥ uₜ
 *
 * <ul>
 *   <li>hₜ = RLE(Xₜ) ∥ BIT₄(Vₜ) ∥ eₜ ∥ kₜ ∥ cₜ ∥ ḋₜ
 *   <li>qₜ = optional mask (if ḋₜ=0): ft flag, optional RLE mask, rt flag
 *   <li>uₜ = compressed bits or full packet
 * </ul>
 */
public final class Decompressor {

  // Configuration (immutable after construction)
  private final int length;
  private final int robustness;
  private final BitVector initialMask;

  // State
  private BitVector mask;
  private BitVector prevOutput;
  private BitVector positiveChanges; // Track positive updates (Xt from decompression)
  private int t;

  // Pre-allocated work buffers (avoid per-packet allocation)
  private final BitVector workXt;
  private final BitVector workMaskDiff;
  private final BitVector workExtractMask;
  private final BitVector workOutput;
  private final int[] scratchBits; // For bitInsert

  /**
   * Creates a new Decompressor.
   *
   * @param length packet length in bits
   * @param initialMask initial mask vector (null for all zeros)
   * @param robustness robustness parameter R (0-7)
   */
  public Decompressor(int length, BitVector initialMask, int robustness) {
    if (length <= 0 || length > Ccsds124.MAX_PACKET_LENGTH) {
      throw new IllegalArgumentException("Invalid packet length: " + length);
    }
    if (robustness < 0 || robustness > Ccsds124.MAX_ROBUSTNESS) {
      throw new IllegalArgumentException("Invalid robustness: " + robustness);
    }

    this.length = length;
    this.robustness = robustness;
    this.initialMask = initialMask != null ? new BitVector(initialMask) : new BitVector(length);

    // Initialize state
    this.mask = new BitVector(this.initialMask);
    this.prevOutput = new BitVector(length);
    this.positiveChanges = new BitVector(length);
    this.t = 0;

    // Pre-allocate work buffers
    this.workXt = new BitVector(length);
    this.workMaskDiff = new BitVector(length);
    this.workExtractMask = new BitVector(length);
    this.workOutput = new BitVector(length);
    this.scratchBits = new int[length]; // Max possible mask bits
  }

  /**
   * Decompresses a single packet.
   *
   * @param reader the bit reader positioned at the start of a compressed packet
   * @return decompressed output as a BitVector
   * @throws Ccsds124Exception if decompression fails
   */
  public BitVector decompressPacket(BitReader reader) throws Ccsds124Exception {
    // Use pre-allocated output buffer, copy previous output as prediction base
    workOutput.copyFrom(prevOutput);

    // Clear positive changes tracker
    positiveChanges.zero();

    // ====================================================================
    // Parse hₜ: Mask change information
    // hₜ = RLE(Xₜ) ∥ BIT₄(Vₜ) ∥ eₜ ∥ kₜ ∥ cₜ ∥ ḋₜ
    // ====================================================================

    // Decode RLE(Xₜ) - mask changes (use pre-allocated buffer)
    Decoder.rleDecodeInto(reader, workXt);

    // Read BIT₄(Vₜ) - effective robustness (4 bits)
    int vt = reader.readBits(4);

    // Process eₜ, kₜ, cₜ if Vₜ > 0 and there are changes
    int ct = 0;
    int changeCount = workXt.hammingWeight();

    if (vt > 0 && changeCount > 0) {
      // Read eₜ
      int et = reader.readBit();

      if (et == 1) {
        // Read kₜ bits into scratch buffer (reuse scratchBits)
        int ktIdx = 0;
        for (int i = 0; i < length && ktIdx < changeCount; i++) {
          if (workXt.getBit(i) != 0) {
            scratchBits[ktIdx++] = reader.readBit();
          }
        }

        // Apply mask updates based on kₜ
        ktIdx = 0;
        for (int i = 0; i < length; i++) {
          if (workXt.getBit(i) != 0) {
            if (scratchBits[ktIdx] != 0) {
              mask.setBit(i, 0);
              positiveChanges.setBit(i, 1);
            } else {
              mask.setBit(i, 1);
            }
            ktIdx++;
          }
        }

        // Read cₜ
        ct = reader.readBit();
      } else {
        // et = 0: all updates are negative (mask bits become 1)
        for (int i = 0; i < length; i++) {
          if (workXt.getBit(i) != 0) {
            mask.setBit(i, 1);
          }
        }
      }
    } else if (vt == 0 && changeCount > 0) {
      // Vt = 0: toggle mask bits at change positions
      for (int i = 0; i < length; i++) {
        if (workXt.getBit(i) != 0) {
          int currentVal = mask.getBit(i);
          mask.setBit(i, currentVal == 0 ? 1 : 0);
        }
      }
    }

    // Read ḋₜ
    int dt = reader.readBit();

    // ====================================================================
    // Parse qₜ: Optional full mask
    // ====================================================================

    int rt = 0;

    if (dt == 0) {
      // Read ft flag
      int ft = reader.readBit();

      if (ft == 1) {
        // Full mask follows: decode RLE(M XOR (M<<)) into pre-allocated buffer
        Decoder.rleDecodeInto(reader, workMaskDiff);

        // Reverse the horizontal XOR to get the actual mask
        int current = workMaskDiff.getBit(length - 1);
        mask.setBit(length - 1, current);

        for (int i = length - 2; i >= 0; i--) {
          int hxorBit = workMaskDiff.getBit(i);
          current = hxorBit ^ current;
          mask.setBit(i, current);
        }
      }

      // Read rt flag
      rt = reader.readBit();
    }

    // ====================================================================
    // Parse uₜ: Data bits
    // ====================================================================

    if (rt == 1) {
      // Full packet follows: COUNT(F) ∥ Iₜ
      Decoder.countDecode(reader);

      // Read full packet
      for (int i = 0; i < length; i++) {
        workOutput.setBit(i, reader.readBit());
      }
    } else {
      // Compressed: extract unpredictable bits using pre-allocated mask
      if (ct == 1 && vt > 0) {
        // BE(Iₜ, (Xₜ OR Mₜ))
        workExtractMask.copyFrom(mask);
        workExtractMask.orInPlace(positiveChanges);
      } else {
        // BE(Iₜ, Mₜ)
        workExtractMask.copyFrom(mask);
      }

      // Insert unpredictable bits using scratch buffer
      Decoder.bitInsert(reader, workOutput, workExtractMask, scratchBits);
    }

    // ====================================================================
    // Update state for next cycle
    // ====================================================================

    prevOutput.copyFrom(workOutput);
    t++;

    // Return a copy of the output (caller may store the result)
    BitVector result = new BitVector(length);
    result.copyFrom(workOutput);
    return result;
  }

  /** Resets the decompressor to initial state. */
  public void reset() {
    mask.copyFrom(initialMask);
    prevOutput.zero();
    positiveChanges.zero();
    t = 0;
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

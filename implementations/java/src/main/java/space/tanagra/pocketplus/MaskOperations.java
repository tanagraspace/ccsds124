/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.pocketplus;

/**
 * Mask and build vector operations for POCKET+ compression.
 *
 * <p>Implements CCSDS 124.0-B-1 Section 4:
 *
 * <ul>
 *   <li>Build Vector Update (Equation 6)
 *   <li>Mask Vector Update (Equation 7)
 *   <li>Change Vector Computation (Equation 8)
 * </ul>
 */
public final class MaskOperations {

  private MaskOperations() {
    // Utility class
  }

  /**
   * Updates the build vector in-place (CCSDS Eq. 6).
   *
   * <ul>
   *   <li>Bₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ (if t &gt; 0 and ṗₜ = 0)
   *   <li>Bₜ = 0 (otherwise: t=0 or ṗₜ=1)
   * </ul>
   *
   * @param build build vector to update in-place
   * @param input current input Iₜ
   * @param prevInput previous input Iₜ₋₁
   * @param newMaskFlag true if starting new mask period (ṗₜ = 1)
   */
  public static void updateBuild(
      BitVector build, BitVector input, BitVector prevInput, boolean newMaskFlag) {
    if (newMaskFlag) {
      build.zero();
    } else {
      BitVector changes = input.xor(prevInput);
      BitVector result = changes.or(build);
      build.copyFrom(result);
    }
  }

  /**
   * Updates the build vector in-place using word-level operations (CCSDS Eq. 6).
   *
   * @param build build vector to update in-place
   * @param input current input Iₜ
   * @param prevInput previous input Iₜ₋₁
   * @param newMaskFlag true if starting new mask period (ṗₜ = 1)
   */
  public static void updateBuildInPlace(
      BitVector build, BitVector input, BitVector prevInput, boolean newMaskFlag) {
    if (newMaskFlag) {
      build.zero();
    } else {
      // Bₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ computed in-place
      int numWords = build.numWords();
      for (int i = 0; i < numWords; i++) {
        int changes = input.getWord(i) ^ prevInput.getWord(i);
        build.setWord(i, changes | build.getWord(i));
      }
    }
  }

  /**
   * Updates the mask vector in-place (CCSDS Eq. 7).
   *
   * <ul>
   *   <li>Mₜ = (Iₜ XOR Iₜ₋₁) OR Mₜ₋₁ (if ṗₜ = 0)
   *   <li>Mₜ = (Iₜ XOR Iₜ₋₁) OR Bₜ₋₁ (if ṗₜ = 1)
   * </ul>
   *
   * @param mask mask vector to update in-place
   * @param input current input Iₜ
   * @param prevInput previous input Iₜ₋₁
   * @param buildPrev previous build vector Bₜ₋₁
   * @param newMaskFlag true if starting new mask period (ṗₜ = 1)
   */
  public static void updateMask(
      BitVector mask,
      BitVector input,
      BitVector prevInput,
      BitVector buildPrev,
      boolean newMaskFlag) {
    BitVector changes = input.xor(prevInput);
    BitVector result = newMaskFlag ? changes.or(buildPrev) : changes.or(mask);
    mask.copyFrom(result);
  }

  /**
   * Updates the mask vector in-place using word-level operations (CCSDS Eq. 7).
   *
   * @param mask mask vector to update in-place
   * @param input current input Iₜ
   * @param prevInput previous input Iₜ₋₁
   * @param buildPrev previous build vector Bₜ₋₁
   * @param newMaskFlag true if starting new mask period (ṗₜ = 1)
   */
  public static void updateMaskInPlace(
      BitVector mask,
      BitVector input,
      BitVector prevInput,
      BitVector buildPrev,
      boolean newMaskFlag) {
    int numWords = mask.numWords();
    for (int i = 0; i < numWords; i++) {
      int changes = input.getWord(i) ^ prevInput.getWord(i);
      if (newMaskFlag) {
        mask.setWord(i, changes | buildPrev.getWord(i));
      } else {
        mask.setWord(i, changes | mask.getWord(i));
      }
    }
  }

  /**
   * Computes the change vector (CCSDS Eq. 8).
   *
   * @param mask current mask vector Mₜ
   * @param prevMask previous mask vector Mₜ₋₁
   * @param t current time index
   * @return change vector Dₜ
   */
  public static BitVector computeChange(BitVector mask, BitVector prevMask, int t) {
    if (t == 0) {
      // CCSDS Eq. 8: D0 = 0 — both encoder and decoder start from the
      // same user-specified M0, so there is no change to communicate
      return new BitVector(mask.length());
    } else {
      return mask.xor(prevMask);
    }
  }

  /**
   * Computes change vector into pre-allocated buffer (CCSDS Eq. 8).
   *
   * @param result pre-allocated buffer for result
   * @param mask current mask vector Mₜ
   * @param prevMask previous mask vector Mₜ₋₁
   * @param t current time index
   */
  public static void computeChangeInto(
      BitVector result, BitVector mask, BitVector prevMask, int t) {
    if (t == 0) {
      // CCSDS Eq. 8: D0 = 0 — no change at initialization
      result.zero();
    } else {
      // Dₜ = Mₜ XOR Mₜ₋₁ computed directly into result
      int numWords = result.numWords();
      for (int i = 0; i < numWords; i++) {
        result.setWord(i, mask.getWord(i) ^ prevMask.getWord(i));
      }
    }
  }
}

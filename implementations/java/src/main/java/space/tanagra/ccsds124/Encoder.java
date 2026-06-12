/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

/**
 * Encoding functions for CCSDS 124.0-B-1 compression.
 *
 * <p>This class provides the COUNT, RLE, and bit extraction encoding functions defined in CCSDS
 * 124.0-B-1 Section 5.2.
 */
public final class Encoder {

  /**
   * Pre-computed COUNT encodings for values 2-33.
   *
   * <p>Values are '110' + BIT5(A-2) = 0xC0 | (A-2)
   */
  private static final int[] COUNT_VALUES = {
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, // 2-9
    0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, // 10-17
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, // 18-25
    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF // 26-33
  };

  /**
   * DeBruijn lookup table for fast LSB finding (matches C reference).
   *
   * <p>Note: Returns 1-indexed positions (1-32), not 0-indexed.
   */
  private static final int[] DEBRUIJN_LOOKUP = {
    1, 2, 29, 3, 30, 15, 25, 4, 31, 23, 21, 16, 26, 18, 5, 9,
    32, 28, 14, 24, 22, 20, 17, 8, 27, 13, 19, 7, 12, 6, 11, 10
  };

  private static final int DEBRUIJN_CONSTANT = 0x077CB531;

  private Encoder() {
    // Utility class
  }

  /**
   * Encodes a count value using COUNT encoding (CCSDS Eq. 9).
   *
   * <p>COUNT encoding:
   *
   * <ul>
   *   <li>A = 1: output '0' (1 bit)
   *   <li>2 &lt;= A &lt;= 33: output '110' followed by 5 bits of (A-2)
   *   <li>A &gt;= 34: output '111' followed by extended encoding
   * </ul>
   *
   * @param output the output buffer
   * @param a the value to encode (must be &gt;= 1)
   */
  public static void countEncode(BitBuffer output, int a) {
    if (a <= 0 || a > 65535) {
      return;
    }

    if (a == 1) {
      // Case 1: A = 1 -> '0'
      output.appendBit(0);
    } else if (a <= 33) {
      // Case 2: 2 <= A <= 33 -> '110' + BIT5(A-2)
      output.appendBits(COUNT_VALUES[a - 2], 8);
    } else {
      // Case 3: A >= 34 -> '111' + BIT_E(A-2)
      // Append '111' prefix (3 bits)
      output.appendBits(0b111, 3);

      // Calculate E = 2 * floor(log2(A-2) + 1) - 6
      int value = a - 2;
      int highestBit = 31 - Integer.numberOfLeadingZeros(value);
      int e = (2 * (highestBit + 1)) - 6;

      // Append BIT_E(A-2) MSB-first using bulk operation
      output.appendBits(value, e);
    }
  }

  /**
   * Encodes a BitVector using RLE encoding (CCSDS Eq. 10).
   *
   * <p>RLE encoding finds each '1' bit from high position to low, and for each outputs COUNT(delta)
   * where delta is the gap (zeros + 1) since the previous '1' bit, then outputs '10' terminator.
   *
   * @param output the output buffer
   * @param input the BitVector to encode
   */
  public static void rleEncode(BitBuffer output, BitVector input) {
    int length = input.length();
    int numWords = input.numWords();

    // Start from the end of the vector
    int oldBitPosition = length;

    // Process words in reverse order (from high to low)
    for (int word = numWords - 1; word >= 0; word--) {
      int wordData = input.getWord(word);

      // Process all set bits in this word
      while (wordData != 0) {
        // Isolate the LSB: x = word & -word
        int lsb = wordData & (-wordData);

        // Find LSB position using DeBruijn sequence
        int debruijnIndex = ((lsb * DEBRUIJN_CONSTANT) >>> 27) & 31;
        int bitPosInWord = DEBRUIJN_LOOKUP[debruijnIndex];

        // Count from the other side (like C reference line 754)
        bitPosInWord = 32 - bitPosInWord;

        // Calculate global bit position (like C reference line 756)
        int newBitPosition = (word * 32) + bitPosInWord;

        // Calculate delta (number of zeros + 1)
        int delta = oldBitPosition - newBitPosition;

        // Encode the count
        countEncode(output, delta);

        // Update old position for next iteration
        oldBitPosition = newBitPosition;

        // Clear the processed bit
        wordData ^= lsb;
      }
    }

    // Append terminator '10'
    output.appendBits(0b10, 2);
  }

  /**
   * Extracts bits from data where mask is 1 (CCSDS Eq. 11).
   *
   * <p>Output order: MSB to LSB (bits are extracted from highest mask position to lowest).
   *
   * @param output the output buffer
   * @param data the data BitVector
   * @param mask the mask BitVector
   */
  public static void bitExtract(BitBuffer output, BitVector data, BitVector mask) {
    if (data.length() != mask.length()) {
      return;
    }

    int numWords = mask.numWords();

    // Process words in REVERSE order (high to low) like RLE
    // This gives bits from highest position to lowest
    for (int word = numWords - 1; word >= 0; word--) {
      int maskWord = mask.getWord(word);
      int dataWord = data.getWord(word);

      while (maskWord != 0) {
        // Isolate LSB
        int lsb = maskWord & (-maskWord);

        // Find LSB position using DeBruijn
        int debruijnIndex = ((lsb * DEBRUIJN_CONSTANT) >>> 27) & 31;
        int bitPosInWord = 32 - DEBRUIJN_LOOKUP[debruijnIndex];

        // Check if within valid length
        int globalPos = (word * 32) + bitPosInWord;
        if (globalPos < data.length()) {
          // Extract and output data bit
          int bit = ((dataWord & lsb) != 0) ? 1 : 0;
          output.appendBit(bit);
        }

        // Clear processed bit
        maskWord ^= lsb;
      }
    }
  }

  /**
   * Extracts bits from data where mask is 1, in forward order.
   *
   * <p>Output order: LSB to MSB (bits from lowest mask position to highest).
   *
   * @param output the output buffer
   * @param data the data BitVector
   * @param mask the mask BitVector
   */
  public static void bitExtractForward(BitBuffer output, BitVector data, BitVector mask) {
    if (data.length() != mask.length()) {
      return;
    }

    int numWords = mask.numWords();

    // Process words in FORWARD order (low to high)
    // Within each word, find MSBs first using CLZ to get bits from lowest to highest position
    for (int word = 0; word < numWords; word++) {
      int maskWord = mask.getWord(word);
      int dataWord = data.getWord(word);

      while (maskWord != 0) {
        // Find MSB position using count leading zeros
        int clz = Integer.numberOfLeadingZeros(maskWord);
        int bitPosInWord = clz; // Physical position from left

        // MSB-first: physical position 0 = bit index 0
        int globalPos = (word * 32) + bitPosInWord;

        if (globalPos < data.length()) {
          // Extract data bit at this position
          int bitMask = 1 << (31 - clz);
          int bit = ((dataWord & bitMask) != 0) ? 1 : 0;
          output.appendBit(bit);
        }

        // Clear the MSB we just processed
        maskWord &= ~(1 << (31 - clz));
      }
    }
  }
}

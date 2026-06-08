/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.pocketplus;

import java.util.Arrays;

/**
 * Fixed-length bit vector with 32-bit word storage.
 *
 * <p>This class provides efficient bit manipulation operations for the POCKET+ algorithm. Uses
 * MSB-first bit ordering matching the ESA/ESOC reference implementation.
 *
 * <p>Bit numbering (CCSDS 124.0-B-1 Section 1.6.1):
 *
 * <ul>
 *   <li>Bit 0 = first transmitted bit (MSB of first byte)
 *   <li>Bit N-1 = last transmitted bit
 * </ul>
 *
 * <p>Word packing (big-endian):
 *
 * <ul>
 *   <li>Word[i] = (Byte[4i] &lt;&lt; 24) | (Byte[4i+1] &lt;&lt; 16) | (Byte[4i+2] &lt;&lt; 8) |
 *       Byte[4i+3]
 *   <li>Bit 0 at position 31 of word 0, Bit 31 at position 0 of word 0
 * </ul>
 */
public final class BitVector {

  /** 32-bit word storage. */
  private final int[] data;

  /** Length in bits. */
  private final int length;

  /** Number of 32-bit words used. */
  private final int numWords;

  /**
   * Creates a new BitVector with the specified number of bits, initialized to zero.
   *
   * @param numBits the number of bits in this vector
   * @throws IllegalArgumentException if numBits is negative or exceeds maximum
   */
  public BitVector(int numBits) {
    if (numBits < 0 || numBits > PocketPlus.MAX_PACKET_LENGTH) {
      throw new IllegalArgumentException("Invalid bit count: " + numBits);
    }
    this.length = numBits;
    int numBytes = (numBits + 7) / 8;
    this.numWords = (numBytes + 3) / 4; // Ceiling division
    this.data = new int[numWords];
  }

  /**
   * Creates a BitVector by copying another BitVector.
   *
   * @param other the BitVector to copy
   */
  public BitVector(BitVector other) {
    this.length = other.length;
    this.numWords = other.numWords;
    this.data = Arrays.copyOf(other.data, other.data.length);
  }

  /**
   * Returns the length of this vector in bits.
   *
   * @return length in bits
   */
  public int length() {
    return length;
  }

  /**
   * Returns the number of 32-bit words.
   *
   * @return number of words
   */
  public int numWords() {
    return numWords;
  }

  /** Sets all bits to zero. */
  public void zero() {
    Arrays.fill(data, 0);
  }

  /**
   * Gets the bit at the specified position.
   *
   * <p>MSB-first: bit 0 is at position 31 in word 0.
   *
   * @param pos bit position (0 = first transmitted bit)
   * @return 0 or 1
   */
  public int getBit(int pos) {
    if (pos < 0 || pos >= length) {
      return 0;
    }
    // Direct bit-to-word mapping (matches C implementation)
    // word_index = pos / 32, bit_in_word = 31 - (pos % 32)
    int wordIndex = pos >>> 5;
    int bitInWord = 31 - (pos & 31);
    return (data[wordIndex] >>> bitInWord) & 1;
  }

  /**
   * Sets the bit at the specified position.
   *
   * <p>MSB-first: bit 0 is at position 31 in word 0.
   *
   * @param pos bit position (0 = first transmitted bit)
   * @param value 0 or 1
   */
  public void setBit(int pos, int value) {
    if (pos < 0 || pos >= length) {
      return;
    }
    // Direct bit-to-word mapping (matches C implementation)
    int wordIndex = pos >>> 5;
    int bitInWord = 31 - (pos & 31);
    if ((value & 1) != 0) {
      data[wordIndex] |= (1 << bitInWord);
    } else {
      data[wordIndex] &= ~(1 << bitInWord);
    }
  }

  /**
   * Returns a new BitVector that is the XOR of this and another vector.
   *
   * @param other the other BitVector
   * @return new BitVector with result
   */
  public BitVector xor(BitVector other) {
    BitVector result = new BitVector(length);
    int words = Math.min(numWords, other.numWords);
    for (int i = 0; i < words; i++) {
      result.data[i] = this.data[i] ^ other.data[i];
    }
    return result;
  }

  /**
   * Returns a new BitVector that is the OR of this and another vector.
   *
   * @param other the other BitVector
   * @return new BitVector with result
   */
  public BitVector or(BitVector other) {
    BitVector result = new BitVector(length);
    int words = Math.min(numWords, other.numWords);
    for (int i = 0; i < words; i++) {
      result.data[i] = this.data[i] | other.data[i];
    }
    return result;
  }

  /**
   * Returns a new BitVector that is the AND of this and another vector.
   *
   * @param other the other BitVector
   * @return new BitVector with result
   */
  public BitVector and(BitVector other) {
    BitVector result = new BitVector(length);
    int words = Math.min(numWords, other.numWords);
    for (int i = 0; i < words; i++) {
      result.data[i] = this.data[i] & other.data[i];
    }
    return result;
  }

  /**
   * Returns a new BitVector that is the bitwise NOT of this vector.
   *
   * @return new BitVector with inverted bits
   */
  public BitVector not() {
    BitVector result = new BitVector(length);
    for (int i = 0; i < numWords; i++) {
      result.data[i] = ~this.data[i];
    }
    // Mask off unused bits in last word with big-endian packing
    if (numWords > 0) {
      int numBytes = (length + 7) / 8;
      int bytesInLastWord = ((numBytes - 1) % 4) + 1;
      int bitsInLastByte = length - ((numBytes - 1) * 8);

      // Create mask for valid bits in big-endian word.
      // MSB-first: valid bits in a partial last byte are at the HIGH end of
      // the byte, so mask 0xFF << (8 - bits) (GOTCHAS #20).
      int mask = 0;
      for (int b = 0; b < bytesInLastWord; b++) {
        int byteMask;
        if (b == bytesInLastWord - 1) {
          byteMask = (0xFF << (8 - bitsInLastByte)) & 0xFF;
        } else {
          byteMask = 0xFF;
        }
        int shiftAmt = (3 - b) * 8;
        mask |= (byteMask << shiftAmt);
      }
      result.data[numWords - 1] &= mask;
    }
    return result;
  }

  /**
   * Returns a new BitVector that is this vector shifted left by one bit.
   *
   * <p>MSB-first with big-endian word packing: Left shift means shift towards MSB (bit 0). Word
   * level: shift each word left by 1, carry high bit from next word.
   *
   * @return new BitVector shifted left
   */
  public BitVector leftShift() {
    BitVector result = new BitVector(length);
    if (numWords > 0) {
      // Process words from first (MSB) to last (LSB)
      for (int i = 0; i < numWords - 1; i++) {
        // Shift current word left by 1, bring in MSB from next word
        result.data[i] = (data[i] << 1) | (data[i + 1] >>> 31);
      }
      // Last word: shift left, LSB becomes 0
      result.data[numWords - 1] = data[numWords - 1] << 1;
    }
    return result;
  }

  /**
   * Returns a new BitVector with bits reversed.
   *
   * @return new BitVector with reversed bits
   */
  public BitVector reverse() {
    BitVector result = new BitVector(length);
    for (int i = 0; i < length; i++) {
      result.setBit(length - 1 - i, getBit(i));
    }
    return result;
  }

  /**
   * Returns the Hamming weight (number of 1 bits).
   *
   * @return count of set bits
   */
  public int hammingWeight() {
    int count = 0;
    for (int i = 0; i < numWords; i++) {
      count += Integer.bitCount(data[i]);
    }
    // Adjust for extra bits in last word
    int numBytes = (length + 7) / 8;
    int extraBits = (numBytes * 8) - length;
    if (extraBits > 0 && numWords > 0) {
      // Count bits in unused portion and subtract
      int lastWord = data[numWords - 1];
      int mask = (1 << extraBits) - 1;
      count -= Integer.bitCount(lastWord & mask);
    }
    return count;
  }

  /**
   * Checks if all bits are zero.
   *
   * @return true if all bits are zero
   */
  public boolean isZero() {
    for (int i = 0; i < numWords; i++) {
      if (data[i] != 0) {
        return false;
      }
    }
    return true;
  }

  /**
   * Creates a BitVector from a byte array (big-endian packing).
   *
   * @param bytes the byte array
   * @param numBits number of bits to use
   * @return new BitVector
   */
  public static BitVector fromBytes(byte[] bytes, int numBits) {
    BitVector result = new BitVector(numBits);
    int numBytes = (numBits + 7) / 8;

    // Pack bytes into 32-bit words (big-endian)
    int j = 4; // Counter for bytes within word (4, 3, 2, 1)
    int bytesToInt = 0;
    int currentWord = 0;

    for (int i = 0; i < numBytes && i < bytes.length; i++) {
      j--;
      bytesToInt |= (bytes[i] & 0xFF) << (j * 8);

      if (j == 0) {
        // Word complete - store it
        result.data[currentWord] = bytesToInt;
        currentWord++;
        bytesToInt = 0;
        j = 4;
      }
    }

    // Handle incomplete final word
    if (j < 4) {
      result.data[currentWord] = bytesToInt;
    }

    return result;
  }

  /**
   * Converts this BitVector to a byte array (big-endian packing).
   *
   * @return byte array representation
   */
  public byte[] toBytes() {
    int numBytes = (length + 7) / 8;
    byte[] result = new byte[numBytes];

    // Extract bytes from 32-bit words (big-endian)
    int byteIndex = 0;
    for (int wordIndex = 0; wordIndex < numWords && byteIndex < numBytes; wordIndex++) {
      int word = data[wordIndex];

      // Extract up to 4 bytes from this word
      for (int j = 3; j >= 0 && byteIndex < numBytes; j--) {
        result[byteIndex] = (byte) ((word >>> (j * 8)) & 0xFF);
        byteIndex++;
      }
    }

    return result;
  }

  /**
   * Checks equality with another BitVector.
   *
   * @param other the other BitVector
   * @return true if equal
   */
  public boolean equals(BitVector other) {
    if (other == null || this.length != other.length) {
      return false;
    }
    return Arrays.equals(this.data, other.data);
  }

  @Override
  public int hashCode() {
    return Arrays.hashCode(data) ^ length;
  }

  @Override
  public boolean equals(Object obj) {
    if (this == obj) {
      return true;
    }
    if (obj instanceof BitVector) {
      return equals((BitVector) obj);
    }
    return false;
  }

  /**
   * Returns a string representation of this vector (for debugging).
   *
   * @return binary string representation
   */
  @Override
  public String toString() {
    StringBuilder sb = new StringBuilder(length);
    for (int i = 0; i < length; i++) {
      sb.append(getBit(i));
    }
    return sb.toString();
  }

  /**
   * Returns the internal word array (for internal use).
   *
   * @return the data array
   */
  int[] getData() {
    return data;
  }

  /**
   * Gets a word from the internal data array.
   *
   * @param index word index
   * @return word value
   */
  int getWord(int index) {
    if (index >= 0 && index < numWords) {
      return data[index];
    }
    return 0;
  }

  /**
   * Sets a word in the internal data array.
   *
   * @param index word index
   * @param value word value
   */
  void setWord(int index, int value) {
    if (index >= 0 && index < numWords) {
      data[index] = value;
    }
  }

  // ============================================================
  // In-place mutation methods (for performance-critical paths)
  // ============================================================

  /**
   * XORs this vector with another in-place.
   *
   * @param other the other BitVector
   */
  public void xorInPlace(BitVector other) {
    int words = Math.min(numWords, other.numWords);
    for (int i = 0; i < words; i++) {
      data[i] ^= other.data[i];
    }
  }

  /**
   * ORs this vector with another in-place.
   *
   * @param other the other BitVector
   */
  public void orInPlace(BitVector other) {
    int words = Math.min(numWords, other.numWords);
    for (int i = 0; i < words; i++) {
      data[i] |= other.data[i];
    }
  }

  /**
   * ANDs this vector with another in-place.
   *
   * @param other the other BitVector
   */
  public void andInPlace(BitVector other) {
    int words = Math.min(numWords, other.numWords);
    for (int i = 0; i < words; i++) {
      data[i] &= other.data[i];
    }
  }

  /**
   * Copies data from another BitVector into this one.
   *
   * @param other the source BitVector
   */
  public void copyFrom(BitVector other) {
    int words = Math.min(numWords, other.numWords);
    for (int i = 0; i < words; i++) {
      data[i] = other.data[i];
    }
    // Zero remaining words if this is larger
    for (int i = words; i < numWords; i++) {
      data[i] = 0;
    }
  }

  /**
   * Left shifts this vector by one bit in-place.
   *
   * <p>MSB-first: shift towards bit 0.
   */
  public void leftShiftInPlace() {
    if (numWords > 0) {
      for (int i = 0; i < numWords - 1; i++) {
        data[i] = (data[i] << 1) | (data[i + 1] >>> 31);
      }
      data[numWords - 1] = data[numWords - 1] << 1;
    }
  }
}

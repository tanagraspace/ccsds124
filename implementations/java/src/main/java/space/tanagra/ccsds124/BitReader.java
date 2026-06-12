/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

/**
 * Sequential bit reader for decompression.
 *
 * <p>This class reads bits from a byte array in big-endian order (MSB first), tracking the current
 * position for sequential parsing.
 */
public final class BitReader {

  /** Source data. */
  private final byte[] data;

  /** Total number of available bits. */
  private final int numBits;

  /** Current bit position. */
  private int position;

  /**
   * Creates a new BitReader.
   *
   * @param data source byte array
   * @param numBits total number of bits to read
   */
  public BitReader(byte[] data, int numBits) {
    this.data = data;
    this.numBits = Math.min(numBits, data.length * 8);
    this.position = 0;
  }

  /**
   * Reads a single bit.
   *
   * @return the bit value (0 or 1)
   * @throws Ccsds124Exception if no bits remaining
   */
  public int readBit() throws Ccsds124Exception {
    if (position >= numBits) {
      throw new Ccsds124Exception("Bit buffer underflow");
    }
    int byteIndex = position / 8;
    int bitOffset = 7 - (position % 8);
    position++;
    return (data[byteIndex] >>> bitOffset) & 1;
  }

  /**
   * Reads multiple bits and returns them as an integer.
   *
   * <p>Optimized to read aligned bytes when possible, falling back to bit-by-bit for unaligned
   * reads.
   *
   * @param count number of bits to read (1-32)
   * @return the bits as an integer
   * @throws Ccsds124Exception if not enough bits remaining
   */
  public int readBits(int count) throws Ccsds124Exception {
    if (count <= 0 || count > 32) {
      throw new Ccsds124Exception("Invalid bit count: " + count);
    }
    if (position + count > numBits) {
      throw new Ccsds124Exception(
          "Bit buffer underflow: need " + count + " bits, have " + remaining());
    }

    int value = 0;
    int remaining = count;

    // Handle leading unaligned bits
    int bitOffset = position & 7;
    if (bitOffset != 0) {
      int bitsInCurrentByte = 8 - bitOffset;
      int bitsToRead = Math.min(bitsInCurrentByte, remaining);
      int byteIndex = position >>> 3;
      int mask = (1 << bitsToRead) - 1;
      int shift = bitsInCurrentByte - bitsToRead;
      value = (data[byteIndex] >>> shift) & mask;
      position += bitsToRead;
      remaining -= bitsToRead;
    }

    // Read full bytes
    while (remaining >= 8) {
      int byteIndex = position >>> 3;
      value = (value << 8) | (data[byteIndex] & 0xFF);
      position += 8;
      remaining -= 8;
    }

    // Handle trailing bits
    if (remaining > 0) {
      int byteIndex = position >>> 3;
      int shift = 8 - remaining;
      value = (value << remaining) | ((data[byteIndex] >>> shift) & ((1 << remaining) - 1));
      position += remaining;
    }

    return value;
  }

  /**
   * Peeks at the next bit without advancing position.
   *
   * @return the bit value (0 or 1), or -1 if no bits remaining
   */
  public int peekBit() {
    if (position >= numBits) {
      return -1;
    }
    int byteIndex = position / 8;
    int bitOffset = 7 - (position % 8);
    return (data[byteIndex] >>> bitOffset) & 1;
  }

  /**
   * Returns the number of bits remaining.
   *
   * @return remaining bits
   */
  public int remaining() {
    return numBits - position;
  }

  /**
   * Returns the current bit position.
   *
   * @return current position
   */
  public int position() {
    return position;
  }

  /**
   * Aligns position to the next byte boundary.
   *
   * @return number of bits skipped
   */
  public int alignByte() {
    int skipped = 0;
    while (position % 8 != 0 && position < numBits) {
      position++;
      skipped++;
    }
    return skipped;
  }
}

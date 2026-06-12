/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

import java.util.Arrays;

/**
 * Variable-length bit buffer for building compressed output.
 *
 * <p>This class accumulates bits during compression, using a 64-bit accumulator for efficient
 * packing. Bits are written in big-endian order (MSB first).
 */
public final class BitBuffer {

  /** Maximum output size in bytes (must accommodate large streams). */
  private static final int MAX_OUTPUT_BYTES = Ccsds124.MAX_PACKET_BYTES * 100;

  /** Output buffer. */
  private byte[] data;

  /** Number of bits written. */
  private int numBits;

  /** 64-bit accumulator for bit packing. */
  private long acc;

  /** Number of bits in the accumulator. */
  private int accBits;

  /** Creates a new empty BitBuffer. */
  public BitBuffer() {
    this.data = new byte[1024]; // Initial capacity
    this.numBits = 0;
    this.acc = 0;
    this.accBits = 0;
  }

  /** Clears the buffer, resetting to empty state. */
  public void clear() {
    numBits = 0;
    acc = 0;
    accBits = 0;
    bytesWritten = 0;
  }

  /**
   * Appends a single bit to the buffer.
   *
   * @param bit the bit value (0 or 1)
   */
  public void appendBit(int bit) {
    acc = (acc << 1) | (bit & 1);
    accBits++;
    numBits++;
    // Flush complete bytes as they accumulate
    if (accBits >= 8) {
      flushAccumulator();
    }
  }

  /**
   * Appends multiple bits from an integer value.
   *
   * @param value the value containing bits (MSB aligned)
   * @param count number of bits to append (1-32)
   */
  public void appendBits(int value, int count) {
    if (count <= 0 || count > 32) {
      return;
    }
    // Mask to the requested number of bits
    long masked = value & ((1L << count) - 1);
    acc = (acc << count) | masked;
    accBits += count;
    numBits += count;
    // Flush complete bytes
    flushAccumulator();
  }

  /**
   * Appends bits from a BitVector.
   *
   * <p>Bits are appended in MSB-first order (bit 0 first, then bit 1, etc.) matching CCSDS bit
   * numbering where bit 0 is the first transmitted bit.
   *
   * @param bv the BitVector to append from
   * @param count number of bits to append
   */
  public void appendBitVector(BitVector bv, int count) {
    // CCSDS MSB-first: append bits in order from bit 0 to bit count-1
    for (int i = 0; i < count; i++) {
      appendBit(bv.getBit(i));
    }
  }

  /**
   * Appends all bits from another BitBuffer.
   *
   * @param other the BitBuffer to append
   */
  public void appendBitBuffer(BitBuffer other) {
    // Flush the other buffer first
    byte[] otherBytes = other.toBytes();
    int otherBits = other.numBits;

    // Append bit by bit
    for (int i = 0; i < otherBits; i++) {
      int byteIndex = i / 8;
      int bitOffset = 7 - (i % 8);
      int bit = (otherBytes[byteIndex] >>> bitOffset) & 1;
      appendBit(bit);
    }
  }

  /** Byte offset tracking - number of complete bytes written to data array. */
  private int bytesWritten;

  /** Flushes complete bytes from the accumulator to the output buffer. */
  private void flushAccumulator() {
    while (accBits >= 8) {
      accBits -= 8;
      int byteValue = (int) ((acc >>> accBits) & 0xFF);
      ensureCapacity(bytesWritten + 1);
      data[bytesWritten] = (byte) byteValue;
      bytesWritten++;
    }
  }

  /**
   * Ensures the data buffer has capacity for the specified number of bytes.
   *
   * @param requiredBytes number of bytes needed
   */
  private void ensureCapacity(int requiredBytes) {
    if (requiredBytes > data.length) {
      int newSize = Math.min(MAX_OUTPUT_BYTES, Math.max(data.length * 2, requiredBytes));
      data = Arrays.copyOf(data, newSize);
    }
  }

  /**
   * Returns the number of bits in the buffer.
   *
   * @return number of bits
   */
  public int length() {
    return numBits;
  }

  /**
   * Converts the buffer to a byte array.
   *
   * @return byte array containing all bits (padded with zeros if necessary)
   */
  public byte[] toBytes() {
    int totalBytes = (numBits + 7) / 8;
    byte[] result = new byte[totalBytes];

    // Copy already-flushed bytes
    System.arraycopy(data, 0, result, 0, bytesWritten);

    // Handle remaining bits in accumulator (less than 8 bits)
    if (accBits > 0 && bytesWritten < totalBytes) {
      int lastByte = (int) ((acc << (8 - accBits)) & 0xFF);
      result[bytesWritten] = (byte) lastByte;
    }

    return result;
  }
}

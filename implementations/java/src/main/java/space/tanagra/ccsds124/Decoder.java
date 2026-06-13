/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

/**
 * Decoding functions for CCSDS 124.0-B-1 decompression.
 *
 * <p>This class provides the COUNT, RLE, and bit insertion decoding functions that reverse the
 * encoding operations defined in CCSDS 124.0-B-1.
 */
public final class Decoder {

  private Decoder() {
    // Utility class
  }

  /**
   * Decodes a COUNT-encoded value from the bit stream.
   *
   * <p>Returns 0 for terminator '10', otherwise returns the decoded count value.
   *
   * @param reader the bit reader
   * @return the decoded value (0 for terminator, >= 1 for count)
   * @throws Ccsds124Exception if decoding fails
   */
  public static int countDecode(BitReader reader) throws Ccsds124Exception {
    int bit0 = reader.readBit();

    if (bit0 == 0) {
      // '0' means value = 1
      return 1;
    }

    // First bit is 1
    int bit1 = reader.readBit();

    if (bit1 == 0) {
      // '10' is terminator
      return 0;
    }

    // '11...'
    int bit2 = reader.readBit();

    if (bit2 == 0) {
      // '110' + 5 bits for values 2-33
      int value = reader.readBits(5);
      return value + 2;
    }

    // '111' + extended encoding for values >= 34
    // Count zeros to determine field size (C uses do-while that includes terminating '1')
    int size = 0;
    int nextBit;
    do {
      nextBit = reader.readBit();
      size++;
    } while (nextBit == 0);

    // Back up one bit since the '1' is part of the value (like C implementation)
    // Since we can't back up, we account for it differently:
    // value_bits = size + 5, but we already consumed the leading '1'
    // So we read (size + 5 - 1) more bits and prepend the '1'
    int numRemainingBits = size + 5 - 1;
    int value = (1 << numRemainingBits) | reader.readBits(numRemainingBits);

    return value + 2;
  }

  /**
   * Decodes an RLE-encoded BitVector from the bit stream.
   *
   * <p>RLE decoding processes from the end of the vector (matching encoding which goes LSB to MSB).
   * Each COUNT value represents a delta (number of zeros + 1 to skip), then sets a '1' bit.
   *
   * @param reader the bit reader
   * @param length the expected length of the output vector
   * @return the decoded BitVector
   * @throws Ccsds124Exception if decoding fails
   */
  public static BitVector rleDecode(BitReader reader, int length) throws Ccsds124Exception {
    BitVector result = new BitVector(length);
    rleDecodeInto(reader, result);
    return result;
  }

  /**
   * Decodes an RLE-encoded BitVector into a pre-allocated buffer.
   *
   * <p>The target buffer is zeroed before decoding. This avoids allocation in hot paths.
   *
   * @param reader the bit reader
   * @param target pre-allocated BitVector to receive the result
   * @throws Ccsds124Exception if decoding fails
   */
  public static void rleDecodeInto(BitReader reader, BitVector target) throws Ccsds124Exception {
    // Zero the target first
    target.zero();

    int length = target.length();
    int bitPosition = length;

    // Read COUNT values until terminator
    int delta = countDecode(reader);

    while (delta != 0) {
      // A delta beyond the remaining bit position means the encoding is
      // invalid for this vector length (GOTCHAS #20): reject it instead of
      // silently skipping.
      if (delta > bitPosition) {
        throw new Ccsds124Exception("Invalid RLE delta: exceeds remaining bit position");
      }
      bitPosition -= delta;
      target.setBit(bitPosition, 1);
      delta = countDecode(reader);
    }
  }

  /**
   * Inserts bits from the stream into a BitVector where mask is 1.
   *
   * <p>Bits are inserted in reverse order (matching BE extraction).
   *
   * @param reader the bit reader
   * @param data the BitVector to fill (modified in place)
   * @param mask the mask indicating positions to fill
   * @throws Ccsds124Exception if not enough bits available
   */
  public static void bitInsert(BitReader reader, BitVector data, BitVector mask)
      throws Ccsds124Exception {
    bitInsert(reader, data, mask, null);
  }

  /**
   * Inserts bits from the stream into a BitVector where mask is 1.
   *
   * <p>Bits are inserted in reverse order (matching BE extraction). Uses a pre-allocated scratch
   * buffer to avoid allocation in hot paths.
   *
   * @param reader the bit reader
   * @param data the BitVector to fill (modified in place)
   * @param mask the mask indicating positions to fill
   * @param scratch pre-allocated int array of at least mask.hammingWeight() size, or null to
   *     allocate
   * @throws Ccsds124Exception if not enough bits available
   */
  public static void bitInsert(BitReader reader, BitVector data, BitVector mask, int[] scratch)
      throws Ccsds124Exception {
    int length = Math.min(data.length(), mask.length());

    // Count mask bits to know how many we need to insert
    int posCount = mask.hammingWeight();
    if (posCount == 0) {
      return;
    }

    // Use scratch buffer or allocate
    int[] bits = (scratch != null && scratch.length >= posCount) ? scratch : new int[posCount];

    // Read all bits we need
    for (int i = 0; i < posCount; i++) {
      bits[i] = reader.readBit();
    }

    // Insert in reverse order: last read bit goes to first mask position
    int bitIdx = posCount - 1;
    for (int i = 0; i < length && bitIdx >= 0; i++) {
      if (mask.getBit(i) != 0) {
        data.setBit(i, bits[bitIdx--]);
      }
    }
  }
}

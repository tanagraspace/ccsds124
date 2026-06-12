/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import org.junit.jupiter.api.Test;

/** Unit tests for Encoder and Decoder. */
class EncoderDecoderTest {

  // ========== COUNT Encoding Tests ==========

  @Test
  void testCountEncodeOne() {
    BitBuffer buffer = new BitBuffer();
    Encoder.countEncode(buffer, 1);
    // COUNT(1) = '0' (1 bit)
    assertEquals(1, buffer.length());
    byte[] bytes = buffer.toBytes();
    assertEquals(0x00, bytes[0] & 0x80); // First bit is 0
  }

  @Test
  void testCountEncodeSmall() {
    // COUNT(2-33) = '110' + 5 bits
    BitBuffer buffer = new BitBuffer();
    Encoder.countEncode(buffer, 2);
    assertEquals(8, buffer.length());

    buffer = new BitBuffer();
    Encoder.countEncode(buffer, 33);
    assertEquals(8, buffer.length());
  }

  @Test
  void testCountEncodeLarge() {
    // COUNT(34+) = '111' + extended
    BitBuffer buffer = new BitBuffer();
    Encoder.countEncode(buffer, 34);
    assertTrue(buffer.length() > 8);

    buffer = new BitBuffer();
    Encoder.countEncode(buffer, 100);
    assertTrue(buffer.length() > 8);
  }

  @Test
  void testCountEncodeInvalid() {
    BitBuffer buffer = new BitBuffer();
    Encoder.countEncode(buffer, 0);
    assertEquals(0, buffer.length()); // Should not encode anything

    buffer = new BitBuffer();
    Encoder.countEncode(buffer, 65536);
    assertEquals(0, buffer.length()); // Above max
  }

  @Test
  void testCountDecodeRoundTrip() throws Ccsds124Exception {
    for (int value : new int[] {1, 2, 10, 33, 34, 100, 500, 1000}) {
      BitBuffer buffer = new BitBuffer();
      Encoder.countEncode(buffer, value);

      BitReader reader = new BitReader(buffer.toBytes(), buffer.length());
      int decoded = Decoder.countDecode(reader);
      assertEquals(value, decoded, "Failed for value: " + value);
    }
  }

  // ========== RLE Encoding Tests ==========

  @Test
  void testRleEncodeAllZeros() {
    BitVector bv = new BitVector(720);
    BitBuffer buffer = new BitBuffer();
    Encoder.rleEncode(buffer, bv);
    // Should just be terminator '10'
    assertEquals(2, buffer.length());
  }

  @Test
  void testRleEncodeSingleBit() {
    BitVector bv = new BitVector(32);
    bv.setBit(15, 1);

    BitBuffer buffer = new BitBuffer();
    Encoder.rleEncode(buffer, bv);
    assertTrue(buffer.length() > 2); // More than just terminator
  }

  @Test
  void testRleDecodeRoundTrip() throws Ccsds124Exception {
    BitVector original = new BitVector(720);
    original.setBit(0, 1);
    original.setBit(100, 1);
    original.setBit(500, 1);
    original.setBit(719, 1);

    BitBuffer buffer = new BitBuffer();
    Encoder.rleEncode(buffer, original);

    BitReader reader = new BitReader(buffer.toBytes(), buffer.length());
    BitVector decoded = Decoder.rleDecode(reader, 720);

    assertTrue(original.equals(decoded));
  }

  // ========== Bit Extract Tests ==========

  @Test
  void testBitExtractAllMasked() {
    BitVector data = new BitVector(8);
    data.setBit(0, 1);
    data.setBit(3, 1);
    data.setBit(7, 1);

    BitVector mask = new BitVector(8);
    // All zeros - nothing to extract

    BitBuffer buffer = new BitBuffer();
    Encoder.bitExtract(buffer, data, mask);
    assertEquals(0, buffer.length());
  }

  @Test
  void testBitExtractSomeMasked() {
    BitVector data = new BitVector(8);
    data.setBit(0, 1);
    data.setBit(3, 1);
    data.setBit(7, 1);

    BitVector mask = new BitVector(8);
    mask.setBit(0, 1);
    mask.setBit(3, 1);

    BitBuffer buffer = new BitBuffer();
    Encoder.bitExtract(buffer, data, mask);
    assertEquals(2, buffer.length()); // Two mask bits
  }

  @Test
  void testBitExtractLengthMismatch() {
    BitVector data = new BitVector(8);
    BitVector mask = new BitVector(16);

    BitBuffer buffer = new BitBuffer();
    Encoder.bitExtract(buffer, data, mask);
    assertEquals(0, buffer.length()); // Should not extract due to mismatch
  }

  // ========== Decoder Tests ==========

  @Test
  void testRleDecodeInto() throws Ccsds124Exception {
    BitVector original = new BitVector(720);
    original.setBit(50, 1);
    original.setBit(200, 1);

    BitBuffer buffer = new BitBuffer();
    Encoder.rleEncode(buffer, original);

    BitReader reader = new BitReader(buffer.toBytes(), buffer.length());
    BitVector target = new BitVector(720);
    Decoder.rleDecodeInto(reader, target);

    assertTrue(original.equals(target));
  }

  @Test
  void testBitInsert() throws Ccsds124Exception {
    // Create data with some bits set
    BitVector data = new BitVector(8);
    data.setBit(2, 1);
    data.setBit(5, 1);

    // Create mask - only extract positions 2 and 5
    BitVector mask = new BitVector(8);
    mask.setBit(2, 1);
    mask.setBit(5, 1);

    // Extract bits
    BitBuffer buffer = new BitBuffer();
    Encoder.bitExtract(buffer, data, mask);

    // Insert back
    BitVector result = new BitVector(8);
    BitReader reader = new BitReader(buffer.toBytes(), buffer.length());
    int[] scratch = new int[8];
    Decoder.bitInsert(reader, result, mask, scratch);

    // Verify extracted bits match
    assertEquals(data.getBit(2), result.getBit(2));
    assertEquals(data.getBit(5), result.getBit(5));
  }

  /**
   * GOTCHAS #21 / issue #92: an RLE delta exceeding the remaining bit position must be rejected,
   * not silently skipped.
   */
  @Test
  void rleDecodeRejectsInvalidDelta() {
    // COUNT(9) = '110'+BIT5(7), then terminator '10' -> 0xC7 0x80. Delta 9 exceeds 8 bits.
    BitReader reader = new BitReader(new byte[] {(byte) 0xC7, (byte) 0x80}, 16);
    assertThrows(Ccsds124Exception.class, () -> Decoder.rleDecode(reader, 8));
  }
}

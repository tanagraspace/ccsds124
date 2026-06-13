/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import org.junit.jupiter.api.Test;

/** Unit tests for BitVector. */
class BitVectorTest {

  @Test
  void testConstructor() {
    BitVector bv = new BitVector(720);
    assertEquals(720, bv.length());
    assertTrue(bv.isZero());
  }

  @Test
  void testSetGetBit() {
    BitVector bv = new BitVector(32);
    bv.setBit(0, 1);
    bv.setBit(7, 1);
    bv.setBit(31, 1);

    assertEquals(1, bv.getBit(0));
    assertEquals(1, bv.getBit(7));
    assertEquals(1, bv.getBit(31));
    assertEquals(0, bv.getBit(1));
    assertEquals(0, bv.getBit(15));
  }

  @Test
  void testZero() {
    BitVector bv = new BitVector(64);
    bv.setBit(10, 1);
    bv.setBit(50, 1);
    assertFalse(bv.isZero());

    bv.zero();
    assertTrue(bv.isZero());
  }

  @Test
  void testXor() {
    BitVector a = new BitVector(8);
    BitVector b = new BitVector(8);

    a.setBit(0, 1);
    a.setBit(2, 1);
    b.setBit(1, 1);
    b.setBit(2, 1);

    BitVector result = a.xor(b);
    assertEquals(1, result.getBit(0));
    assertEquals(1, result.getBit(1));
    assertEquals(0, result.getBit(2));
  }

  @Test
  void testOr() {
    BitVector a = new BitVector(8);
    BitVector b = new BitVector(8);

    a.setBit(0, 1);
    a.setBit(2, 1);
    b.setBit(1, 1);
    b.setBit(2, 1);

    BitVector result = a.or(b);
    assertEquals(1, result.getBit(0));
    assertEquals(1, result.getBit(1));
    assertEquals(1, result.getBit(2));
    assertEquals(0, result.getBit(3));
  }

  @Test
  void testAnd() {
    BitVector a = new BitVector(8);
    BitVector b = new BitVector(8);

    a.setBit(0, 1);
    a.setBit(2, 1);
    b.setBit(1, 1);
    b.setBit(2, 1);

    BitVector result = a.and(b);
    assertEquals(0, result.getBit(0));
    assertEquals(0, result.getBit(1));
    assertEquals(1, result.getBit(2));
  }

  @Test
  void testNot() {
    BitVector bv = new BitVector(8);
    bv.setBit(0, 1);
    bv.setBit(7, 1);

    BitVector result = bv.not();
    assertEquals(0, result.getBit(0));
    assertEquals(1, result.getBit(1));
    assertEquals(1, result.getBit(6));
    assertEquals(0, result.getBit(7));
  }

  // GOTCHAS #19 (issue #103): NOT of a non-byte-aligned vector must set the
  // valid (high) bits and leave padding zero. NOT of an all-zero length-12
  // vector: positions 0..12 must all be 1.
  @Test
  void testNotMsbAlignedNonByteAligned() {
    BitVector bv = new BitVector(12);
    BitVector result = bv.not();
    for (int i = 0; i < 12; i++) {
      assertEquals(1, result.getBit(i), "position " + i + " should be set");
    }
    assertEquals(12, result.hammingWeight());
  }

  @Test
  void testLeftShift() {
    // MSB-first: leftShift moves bits towards bit 0, bit 0 is lost
    BitVector bv = new BitVector(8);
    bv.setBit(1, 1); // This becomes bit 0 after shift
    bv.setBit(4, 1); // This becomes bit 3 after shift

    BitVector result = bv.leftShift();
    assertEquals(1, result.getBit(0)); // Was bit 1
    assertEquals(0, result.getBit(1)); // Was bit 2 = 0
    assertEquals(1, result.getBit(3)); // Was bit 4
    assertEquals(0, result.getBit(7)); // LSB always becomes 0
  }

  @Test
  void testReverse() {
    BitVector bv = new BitVector(8);
    bv.setBit(0, 1);
    bv.setBit(7, 1);

    BitVector result = bv.reverse();
    assertEquals(1, result.getBit(0));
    assertEquals(1, result.getBit(7));
  }

  @Test
  void testHammingWeight() {
    BitVector bv = new BitVector(32);
    assertEquals(0, bv.hammingWeight());

    bv.setBit(0, 1);
    bv.setBit(10, 1);
    bv.setBit(31, 1);
    assertEquals(3, bv.hammingWeight());
  }

  @Test
  void testFromBytesToBytes() {
    byte[] input = {(byte) 0xAB, (byte) 0xCD};
    BitVector bv = BitVector.fromBytes(input, 16);

    byte[] output = bv.toBytes();
    assertArrayEquals(input, output);
  }

  @Test
  void testEquality() {
    BitVector a = new BitVector(32);
    BitVector b = new BitVector(32);

    a.setBit(5, 1);
    b.setBit(5, 1);

    assertTrue(a.equals(b));
    assertEquals(a.hashCode(), b.hashCode());

    b.setBit(10, 1);
    assertFalse(a.equals(b));
  }

  @Test
  void testCopyConstructor() {
    BitVector original = new BitVector(64);
    original.setBit(10, 1);
    original.setBit(50, 1);

    BitVector copy = new BitVector(original);
    assertTrue(original.equals(copy));

    copy.setBit(30, 1);
    assertFalse(original.equals(copy));
  }

  @Test
  void testToString() {
    BitVector bv = new BitVector(8);
    bv.setBit(0, 1);
    bv.setBit(7, 1);

    String str = bv.toString();
    assertEquals(8, str.length());
    assertEquals('1', str.charAt(0));
    assertEquals('1', str.charAt(7));
  }

  // ========== In-place Operations Tests ==========

  @Test
  void testXorInPlace() {
    BitVector a = new BitVector(8);
    BitVector b = new BitVector(8);

    a.setBit(0, 1);
    a.setBit(2, 1);
    b.setBit(1, 1);
    b.setBit(2, 1);

    a.xorInPlace(b);
    assertEquals(1, a.getBit(0));
    assertEquals(1, a.getBit(1));
    assertEquals(0, a.getBit(2));
  }

  @Test
  void testOrInPlace() {
    BitVector a = new BitVector(8);
    BitVector b = new BitVector(8);

    a.setBit(0, 1);
    b.setBit(1, 1);

    a.orInPlace(b);
    assertEquals(1, a.getBit(0));
    assertEquals(1, a.getBit(1));
  }

  @Test
  void testAndInPlace() {
    BitVector a = new BitVector(8);
    BitVector b = new BitVector(8);

    a.setBit(0, 1);
    a.setBit(1, 1);
    b.setBit(1, 1);

    a.andInPlace(b);
    assertEquals(0, a.getBit(0));
    assertEquals(1, a.getBit(1));
  }

  @Test
  void testCopyFrom() {
    BitVector src = new BitVector(64);
    src.setBit(10, 1);
    src.setBit(50, 1);

    BitVector dest = new BitVector(64);
    dest.copyFrom(src);

    assertTrue(src.equals(dest));
  }

  @Test
  void testCopyFromDifferentSize() {
    BitVector src = new BitVector(32);
    src.setBit(10, 1);

    BitVector dest = new BitVector(64);
    dest.setBit(50, 1);
    dest.copyFrom(src);

    assertEquals(1, dest.getBit(10));
    assertEquals(0, dest.getBit(50)); // Should be cleared
  }

  @Test
  void testLeftShiftInPlace() {
    BitVector bv = new BitVector(8);
    bv.setBit(1, 1);
    bv.setBit(4, 1);

    bv.leftShiftInPlace();
    assertEquals(1, bv.getBit(0));
    assertEquals(0, bv.getBit(1));
    assertEquals(1, bv.getBit(3));
    assertEquals(0, bv.getBit(7));
  }

  // ========== Boundary Tests ==========

  @Test
  void testGetBitOutOfBounds() {
    BitVector bv = new BitVector(8);
    assertEquals(0, bv.getBit(-1));
    assertEquals(0, bv.getBit(8));
    assertEquals(0, bv.getBit(100));
  }

  @Test
  void testSetBitOutOfBounds() {
    BitVector bv = new BitVector(8);
    bv.setBit(-1, 1); // Should not throw
    bv.setBit(8, 1); // Should not throw
    bv.setBit(100, 1); // Should not throw

    // Vector should still be zero (out of bounds sets ignored)
    assertTrue(bv.isZero());
  }

  @Test
  void testInvalidBitCount() {
    assertThrows(IllegalArgumentException.class, () -> new BitVector(-1));
    assertThrows(
        IllegalArgumentException.class, () -> new BitVector(Ccsds124.MAX_PACKET_LENGTH + 1));
  }

  @Test
  void testEqualsWithNull() {
    BitVector bv = new BitVector(8);
    assertFalse(bv.equals((BitVector) null));
  }

  @Test
  void testEqualsWithDifferentLength() {
    BitVector a = new BitVector(8);
    BitVector b = new BitVector(16);
    assertFalse(a.equals(b));
  }

  @Test
  void testEqualsWithObject() {
    BitVector bv = new BitVector(8);
    assertFalse(bv.equals("not a bitvector"));
    assertTrue(bv.equals(bv)); // Same reference
  }

  // ========== Multi-word Tests ==========

  @Test
  void testMultiWordOperations() {
    BitVector bv = new BitVector(720); // 23 words
    bv.setBit(0, 1);
    bv.setBit(31, 1);
    bv.setBit(32, 1);
    bv.setBit(719, 1);

    assertEquals(4, bv.hammingWeight());
    assertEquals(1, bv.getBit(0));
    assertEquals(1, bv.getBit(31));
    assertEquals(1, bv.getBit(32));
    assertEquals(1, bv.getBit(719));
  }

  @Test
  void testLargeFromBytesToBytes() {
    byte[] input = new byte[90]; // 720 bits
    for (int i = 0; i < input.length; i++) {
      input[i] = (byte) i;
    }

    BitVector bv = BitVector.fromBytes(input, 720);
    byte[] output = bv.toBytes();

    assertArrayEquals(input, output);
  }

  @Test
  void testNumWords() {
    assertEquals(1, new BitVector(1).numWords());
    assertEquals(1, new BitVector(32).numWords());
    assertEquals(2, new BitVector(33).numWords());
    assertEquals(23, new BitVector(720).numWords());
  }
}

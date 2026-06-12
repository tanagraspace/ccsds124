/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import org.junit.jupiter.api.Test;

/** Unit tests for BitBuffer and BitReader. */
class BitBufferReaderTest {

  // ========== BitBuffer Tests ==========

  @Test
  void testBitBufferEmpty() {
    BitBuffer buffer = new BitBuffer();
    assertEquals(0, buffer.length());
    byte[] bytes = buffer.toBytes();
    assertEquals(0, bytes.length);
  }

  @Test
  void testBitBufferAppendBit() {
    BitBuffer buffer = new BitBuffer();
    buffer.appendBit(1);
    buffer.appendBit(0);
    buffer.appendBit(1);
    buffer.appendBit(1);

    assertEquals(4, buffer.length());
    byte[] bytes = buffer.toBytes();
    assertEquals(1, bytes.length);
    assertEquals((byte) 0xB0, bytes[0]); // 1011 0000
  }

  @Test
  void testBitBufferAppendBits() {
    BitBuffer buffer = new BitBuffer();
    buffer.appendBits(0b1010, 4);
    buffer.appendBits(0b1100, 4);

    assertEquals(8, buffer.length());
    byte[] bytes = buffer.toBytes();
    assertEquals(1, bytes.length);
    assertEquals((byte) 0xAC, bytes[0]); // 1010 1100
  }

  @Test
  void testBitBufferAppendBitsLarge() {
    BitBuffer buffer = new BitBuffer();
    buffer.appendBits(0xABCD1234, 32);

    assertEquals(32, buffer.length());
    byte[] bytes = buffer.toBytes();
    assertEquals(4, bytes.length);
    assertEquals((byte) 0xAB, bytes[0]);
    assertEquals((byte) 0xCD, bytes[1]);
    assertEquals((byte) 0x12, bytes[2]);
    assertEquals((byte) 0x34, bytes[3]);
  }

  @Test
  void testBitBufferAppendBitsInvalidCount() {
    BitBuffer buffer = new BitBuffer();
    buffer.appendBits(0xFF, 0);
    assertEquals(0, buffer.length());

    buffer.appendBits(0xFF, -1);
    assertEquals(0, buffer.length());

    buffer.appendBits(0xFF, 33);
    assertEquals(0, buffer.length());
  }

  @Test
  void testBitBufferAppendBitVector() {
    BitVector bv = new BitVector(8);
    bv.setBit(0, 1);
    bv.setBit(3, 1);
    bv.setBit(7, 1);

    BitBuffer buffer = new BitBuffer();
    buffer.appendBitVector(bv, 8);

    assertEquals(8, buffer.length());
    byte[] bytes = buffer.toBytes();
    assertEquals(1, bytes.length);
    // MSB-first: bit0=1, bit3=1, bit7=1 -> 10010001 = 0x91
    assertEquals((byte) 0x91, bytes[0]);
  }

  @Test
  void testBitBufferAppendBitBuffer() {
    BitBuffer first = new BitBuffer();
    first.appendBits(0b1010, 4);

    BitBuffer second = new BitBuffer();
    second.appendBits(0b1100, 4);

    first.appendBitBuffer(second);
    assertEquals(8, first.length());
  }

  @Test
  void testBitBufferClear() {
    BitBuffer buffer = new BitBuffer();
    buffer.appendBits(0xFFFF, 16);
    assertEquals(16, buffer.length());

    buffer.clear();
    assertEquals(0, buffer.length());
    assertEquals(0, buffer.toBytes().length);
  }

  // ========== BitReader Tests ==========

  @Test
  void testBitReaderReadBit() throws Ccsds124Exception {
    byte[] data = {(byte) 0xA5}; // 1010 0101
    BitReader reader = new BitReader(data, 8);

    assertEquals(1, reader.readBit());
    assertEquals(0, reader.readBit());
    assertEquals(1, reader.readBit());
    assertEquals(0, reader.readBit());
    assertEquals(0, reader.readBit());
    assertEquals(1, reader.readBit());
    assertEquals(0, reader.readBit());
    assertEquals(1, reader.readBit());
  }

  @Test
  void testBitReaderReadBits() throws Ccsds124Exception {
    byte[] data = {(byte) 0xAB, (byte) 0xCD};
    BitReader reader = new BitReader(data, 16);

    assertEquals(0xA, reader.readBits(4));
    assertEquals(0xB, reader.readBits(4));
    assertEquals(0xC, reader.readBits(4));
    assertEquals(0xD, reader.readBits(4));
  }

  @Test
  void testBitReaderReadBitsLarge() throws Ccsds124Exception {
    byte[] data = {(byte) 0xAB, (byte) 0xCD, (byte) 0x12, (byte) 0x34};
    BitReader reader = new BitReader(data, 32);

    assertEquals(0xABCD1234, reader.readBits(32));
  }

  @Test
  void testBitReaderRemaining() throws Ccsds124Exception {
    byte[] data = {(byte) 0xFF};
    BitReader reader = new BitReader(data, 8);

    assertEquals(8, reader.remaining());
    reader.readBits(3);
    assertEquals(5, reader.remaining());
    reader.readBits(5);
    assertEquals(0, reader.remaining());
  }

  @Test
  void testBitReaderPosition() throws Ccsds124Exception {
    byte[] data = {(byte) 0xFF, (byte) 0xFF};
    BitReader reader = new BitReader(data, 16);

    assertEquals(0, reader.position());
    reader.readBits(5);
    assertEquals(5, reader.position());
    reader.readBit();
    assertEquals(6, reader.position());
  }

  @Test
  void testBitReaderUnderflow() {
    byte[] data = {(byte) 0xFF};
    BitReader reader = new BitReader(data, 8);

    assertDoesNotThrow(() -> reader.readBits(8));
    assertThrows(Ccsds124Exception.class, () -> reader.readBit());
  }

  @Test
  void testBitReaderReadBitsZero() {
    byte[] data = {(byte) 0xFF};
    BitReader reader = new BitReader(data, 8);

    // Reading 0 bits throws exception (invalid count)
    assertThrows(Ccsds124Exception.class, () -> reader.readBits(0));
  }

  @Test
  void testBitReaderUnaligned() throws Ccsds124Exception {
    byte[] data = {(byte) 0xFF, (byte) 0x00};
    BitReader reader = new BitReader(data, 13); // Unaligned bit count

    assertEquals(13, reader.remaining());
    assertEquals(0x1F, reader.readBits(5)); // 11111
    assertEquals(0xE0, reader.readBits(8)); // 11100000 (crosses byte boundary)
  }
}

/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import org.junit.jupiter.api.Test;

/** Unit tests for Ccsds124 high-level API. */
class Ccsds124Test {

  @Test
  void testVersion() {
    String version = Ccsds124.version();
    assertNotNull(version);
    assertTrue(version.matches("\\d+\\.\\d+\\.\\d+"));
    assertEquals("1.0.0", version);
  }

  @Test
  void testCompressNullInput() {
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.compress(null, 90, 2, 20, 50, 100));
  }

  @Test
  void testCompressInvalidPacketSize() {
    byte[] data = new byte[90];
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.compress(data, 0, 2, 20, 50, 100));
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.compress(data, -1, 2, 20, 50, 100));
  }

  @Test
  void testCompressInvalidRobustness() {
    byte[] data = new byte[90];
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.compress(data, 90, -1, 20, 50, 100));
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.compress(data, 90, 8, 20, 50, 100));
  }

  @Test
  void testCompressNonMultiplePacketSize() {
    byte[] data = new byte[100]; // Not a multiple of 90
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.compress(data, 90, 2, 20, 50, 100));
  }

  @Test
  void testDecompressNullInput() {
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.decompress(null, 90, 2));
  }

  @Test
  void testDecompressInvalidPacketSize() {
    byte[] data = new byte[100];
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.decompress(data, 0, 2));
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.decompress(data, -1, 2));
  }

  @Test
  void testDecompressInvalidRobustness() {
    byte[] data = new byte[100];
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.decompress(data, 90, -1));
    assertThrows(Ccsds124Exception.class, () -> Ccsds124.decompress(data, 90, 8));
  }

  @Test
  void testSimpleRoundTrip() throws Ccsds124Exception {
    // Create simple test data - 2 identical packets
    byte[] original = new byte[180]; // 2 packets of 90 bytes
    for (int i = 0; i < original.length; i++) {
      original[i] = (byte) (i % 256);
    }

    // Compress
    byte[] compressed = Ccsds124.compress(original, 90, 1, 10, 20, 50);
    assertNotNull(compressed);
    assertTrue(compressed.length > 0);

    // Decompress
    byte[] decompressed = Ccsds124.decompress(compressed, 90, 1);
    assertNotNull(decompressed);

    // Verify round-trip
    assertArrayEquals(original, decompressed);
  }

  @Test
  void testConstants() {
    assertEquals(65535, Ccsds124.MAX_PACKET_LENGTH);
    assertEquals(8192, Ccsds124.MAX_PACKET_BYTES); // ceil(65535/8) = 8192 bytes
    assertEquals(7, Ccsds124.MAX_ROBUSTNESS);
  }

  // ========== Compressor validation tests ==========

  @Test
  void testCompressorInvalidLength() {
    assertThrows(IllegalArgumentException.class, () -> new Compressor(0, null, 1, 10, 20, 50));
    assertThrows(IllegalArgumentException.class, () -> new Compressor(-1, null, 1, 10, 20, 50));
    assertThrows(
        IllegalArgumentException.class,
        () -> new Compressor(Ccsds124.MAX_PACKET_LENGTH + 1, null, 1, 10, 20, 50));
  }

  @Test
  void testCompressorInvalidRobustness() {
    assertThrows(IllegalArgumentException.class, () -> new Compressor(720, null, -1, 10, 20, 50));
    assertThrows(IllegalArgumentException.class, () -> new Compressor(720, null, 8, 10, 20, 50));
  }

  @Test
  void testCompressorValidBoundary() {
    // Should not throw - boundary values
    new Compressor(1, null, 0, 10, 20, 50);
    new Compressor(Ccsds124.MAX_PACKET_LENGTH, null, 7, 10, 20, 50);
  }

  // ========== Decompressor validation tests ==========

  @Test
  void testDecompressorInvalidLength() {
    assertThrows(IllegalArgumentException.class, () -> new Decompressor(0, null, 1));
    assertThrows(IllegalArgumentException.class, () -> new Decompressor(-1, null, 1));
    assertThrows(
        IllegalArgumentException.class,
        () -> new Decompressor(Ccsds124.MAX_PACKET_LENGTH + 1, null, 1));
  }

  @Test
  void testDecompressorInvalidRobustness() {
    assertThrows(IllegalArgumentException.class, () -> new Decompressor(720, null, -1));
    assertThrows(IllegalArgumentException.class, () -> new Decompressor(720, null, 8));
  }

  @Test
  void testDecompressorValidBoundary() {
    // Should not throw - boundary values
    new Decompressor(1, null, 0);
    new Decompressor(Ccsds124.MAX_PACKET_LENGTH, null, 7);
  }

  @Test
  void testDecompressorReset() {
    Decompressor decomp = new Decompressor(720, null, 1);
    assertEquals(0, decomp.getTimeIndex());
    decomp.reset();
    assertEquals(0, decomp.getTimeIndex());
  }

  @Test
  void testCompressorReset() {
    Compressor comp = new Compressor(720, null, 1, 10, 20, 50);
    assertEquals(0, comp.getTimeIndex());
    comp.reset();
    assertEquals(0, comp.getTimeIndex());
  }
}

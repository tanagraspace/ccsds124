/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;

/**
 * Test against reference test vectors.
 *
 * <p>These tests validate that the Java implementation produces byte-identical output to the ESA
 * reference implementation.
 */
class VectorsTest {

  private static final String TEST_VECTORS_DIR = "../../test-vectors";
  private static final int PACKET_SIZE = 90;

  /** Test vector parameters. */
  private static class Vector {
    final String name;
    final int pt;
    final int ft;
    final int rt;
    final int robustness;
    final String expectedMd5;
    final int expectedSize;

    Vector(String name, int pt, int ft, int rt, int robustness, String expectedMd5, int size) {
      this.name = name;
      this.pt = pt;
      this.ft = ft;
      this.rt = rt;
      this.robustness = robustness;
      this.expectedMd5 = expectedMd5;
      this.expectedSize = size;
    }
  }

  private static final Vector[] VECTORS = {
    new Vector("simple", 10, 20, 50, 1, "ffcfdd063eec899119f1958422223b19", 641),
    new Vector("hiro", 10, 20, 50, 7, "823eed833f5f665f5c9a5d5fe2be559e", 1533),
    new Vector("housekeeping", 20, 50, 100, 2, null, 223078),
    new Vector("edge-cases", 10, 20, 50, 1, null, 10124),
    new Vector("venus-express", 20, 50, 100, 2, null, 5891500),
  };

  private static Path inputDir;
  private static Path expectedDir;
  private static boolean vectorsAvailable;

  @BeforeAll
  static void setup() {
    Path base = Paths.get(TEST_VECTORS_DIR);
    inputDir = base.resolve("input");
    expectedDir = base.resolve("expected-output");
    vectorsAvailable = Files.isDirectory(inputDir) && Files.isDirectory(expectedDir);
    if (!vectorsAvailable) {
      System.err.println("Warning: Test vectors not found at " + base.toAbsolutePath());
    }
  }

  @Test
  void testSimpleVector() throws Exception {
    testVector(VECTORS[0]);
  }

  @Test
  void testHiroVector() throws Exception {
    testVector(VECTORS[1]);
  }

  @Test
  void testHousekeepingVector() throws Exception {
    testVector(VECTORS[2]);
  }

  @Test
  void testEdgeCasesVector() throws Exception {
    testVector(VECTORS[3]);
  }

  @Test
  void testSimpleRoundTrip() throws Exception {
    testRoundTrip(VECTORS[0]);
  }

  @Test
  void testHiroRoundTrip() throws Exception {
    testRoundTrip(VECTORS[1]);
  }

  @Test
  void testHousekeepingRoundTrip() throws Exception {
    testRoundTrip(VECTORS[2]);
  }

  @Test
  void testEdgeCasesRoundTrip() throws Exception {
    testRoundTrip(VECTORS[3]);
  }

  @Test
  void testVenusExpressVector() throws Exception {
    testVector(VECTORS[4]);
  }

  @Test
  void testVenusExpressRoundTrip() throws Exception {
    testRoundTrip(VECTORS[4]);
  }

  private void testVector(Vector v) throws IOException, Ccsds124Exception {
    if (!vectorsAvailable) {
      System.out.println("Skipping " + v.name + " - test vectors not available");
      return;
    }

    // Read input
    String inputName = v.name.equals("venus-express") ? v.name + ".ccsds" : v.name + ".bin";
    Path inputPath = inputDir.resolve(inputName);
    if (!Files.exists(inputPath)) {
      System.out.println("Skipping " + v.name + " - input file not found");
      return;
    }
    byte[] input = Files.readAllBytes(inputPath);

    // Compress
    byte[] compressed = Ccsds124.compress(input, PACKET_SIZE, v.robustness, v.pt, v.ft, v.rt);

    // Read expected
    String outputName = inputName + ".pkt";
    Path expectedPath = expectedDir.resolve(outputName);
    if (!Files.exists(expectedPath)) {
      System.out.println("Skipping " + v.name + " - expected output not found");
      return;
    }
    byte[] expected = Files.readAllBytes(expectedPath);

    // Compare size
    assertEquals(
        expected.length,
        compressed.length,
        String.format(
            "%s: size mismatch - expected %d, got %d", v.name, expected.length, compressed.length));

    // Compare bytes
    assertArrayEquals(expected, compressed, v.name + ": byte mismatch with reference");

    // Verify MD5 if available
    if (v.expectedMd5 != null) {
      String actualMd5 = md5Hex(compressed);
      assertEquals(v.expectedMd5, actualMd5, v.name + ": MD5 mismatch");
    }

    System.out.printf(
        "%s: OK (compressed %d -> %d bytes)%n", v.name, input.length, compressed.length);
  }

  private void testRoundTrip(Vector v) throws IOException, Ccsds124Exception {
    if (!vectorsAvailable) {
      System.out.println("Skipping round-trip " + v.name + " - test vectors not available");
      return;
    }

    // Read input
    String inputName = v.name.equals("venus-express") ? v.name + ".ccsds" : v.name + ".bin";
    Path inputPath = inputDir.resolve(inputName);
    if (!Files.exists(inputPath)) {
      System.out.println("Skipping round-trip " + v.name + " - input file not found");
      return;
    }
    byte[] original = Files.readAllBytes(inputPath);

    // Compress
    byte[] compressed = Ccsds124.compress(original, PACKET_SIZE, v.robustness, v.pt, v.ft, v.rt);

    // Decompress
    byte[] decompressed = Ccsds124.decompress(compressed, PACKET_SIZE, v.robustness);

    // Verify
    assertEquals(
        original.length,
        decompressed.length,
        String.format(
            "%s round-trip: size mismatch - expected %d, got %d",
            v.name, original.length, decompressed.length));

    assertArrayEquals(original, decompressed, v.name + " round-trip: decompressed != original");

    System.out.printf("%s: round-trip OK%n", v.name);
  }

  private static String md5Hex(byte[] data) {
    try {
      MessageDigest md = MessageDigest.getInstance("MD5");
      byte[] digest = md.digest(data);
      StringBuilder sb = new StringBuilder();
      for (byte b : digest) {
        sb.append(String.format("%02x", b & 0xFF));
      }
      return sb.toString();
    } catch (NoSuchAlgorithmException e) {
      throw new RuntimeException("MD5 not available", e);
    }
  }
}

/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124.cli;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

import space.tanagra.ccsds124.Ccsds124;

/**
 * Performance benchmarks for CCSDS 124.0-B-1 compression.
 *
 * <p>Measures compression throughput for regression testing during development. Note: Desktop
 * performance differs from embedded targets - use for relative comparisons only.
 *
 * <p>Usage:
 *
 * <pre>
 *   java -jar ccsds124.jar bench          # Run with default 100 iterations
 *   java -jar ccsds124.jar bench 1000     # Run with custom iteration count
 * </pre>
 */
public final class Bench {

  private static final int DEFAULT_ITERATIONS = 100;
  private static final int PACKET_SIZE_BITS = 720;
  private static final int PACKET_SIZE_BYTES = PACKET_SIZE_BITS / 8;

  /** Benchmark configuration. */
  private static final BenchConfig[] BENCHMARKS = {
    new BenchConfig("simple", "../../test-vectors/input/simple.bin", 1, 10, 20, 50),
    new BenchConfig("hiro", "../../test-vectors/input/hiro.bin", 7, 10, 20, 50),
    new BenchConfig("housekeeping", "../../test-vectors/input/housekeeping.bin", 2, 20, 50, 100),
    new BenchConfig(
        "venus-express", "../../test-vectors/input/venus-express.ccsds", 2, 20, 50, 100),
  };

  private Bench() {
    // Utility class
  }

  /** Benchmark configuration record. */
  private static class BenchConfig {
    final String name;
    final String path;
    final int robustness;
    final int pt;
    final int ft;
    final int rt;

    BenchConfig(String name, String path, int robustness, int pt, int ft, int rt) {
      this.name = name;
      this.path = path;
      this.robustness = robustness;
      this.pt = pt;
      this.ft = ft;
      this.rt = rt;
    }
  }

  private static byte[] loadFile(String path) {
    try {
      Path filePath = Paths.get(path);
      return Files.readAllBytes(filePath);
    } catch (IOException e) {
      return null;
    }
  }

  private static void benchCompress(BenchConfig config, int iterations) {
    byte[] input = loadFile(config.path);
    if (input == null) {
      System.out.printf("%-20s SKIP (file not found)%n", config.name);
      return;
    }

    int numPackets = input.length / PACKET_SIZE_BYTES;

    // Warmup run
    try {
      Ccsds124.compress(
          input, PACKET_SIZE_BYTES, config.robustness, config.pt, config.ft, config.rt);
    } catch (Exception e) {
      System.out.printf("%-20s SKIP (warmup failed: %s)%n", config.name, e.getMessage());
      return;
    }

    // Benchmark
    long startNanos = System.nanoTime();

    for (int i = 0; i < iterations; i++) {
      try {
        Ccsds124.compress(
            input, PACKET_SIZE_BYTES, config.robustness, config.pt, config.ft, config.rt);
      } catch (Exception e) {
        System.out.printf("%-20s SKIP (iteration %d failed)%n", config.name, i);
        return;
      }
    }

    long endNanos = System.nanoTime();

    double totalUs = (endNanos - startNanos) / 1000.0;
    double perIterUs = totalUs / iterations;
    double perPacketUs = perIterUs / numPackets;
    double throughputKbps = (input.length * 8.0 * 1000.0) / perIterUs;

    System.out.printf(
        "%-20s %8.2f \u00b5s/iter  %6.2f \u00b5s/pkt  %8.1f Kbps  (%d pkts)%n",
        config.name, perIterUs, perPacketUs, throughputKbps, numPackets);
  }

  private static void benchDecompress(BenchConfig config, int iterations) {
    byte[] input = loadFile(config.path);
    if (input == null) {
      System.out.printf("%-20s SKIP (file not found)%n", config.name);
      return;
    }

    int numPackets = input.length / PACKET_SIZE_BYTES;

    // First compress the data
    byte[] compressed;
    try {
      compressed =
          Ccsds124.compress(
              input, PACKET_SIZE_BYTES, config.robustness, config.pt, config.ft, config.rt);
    } catch (Exception e) {
      System.out.printf("%-20s SKIP (compression failed: %s)%n", config.name, e.getMessage());
      return;
    }

    // Warmup run
    try {
      Ccsds124.decompress(compressed, PACKET_SIZE_BYTES, config.robustness);
    } catch (Exception e) {
      System.out.printf("%-20s SKIP (warmup failed: %s)%n", config.name, e.getMessage());
      return;
    }

    // Benchmark
    long startNanos = System.nanoTime();

    for (int i = 0; i < iterations; i++) {
      try {
        Ccsds124.decompress(compressed, PACKET_SIZE_BYTES, config.robustness);
      } catch (Exception e) {
        System.out.printf("%-20s SKIP (iteration %d failed)%n", config.name, i);
        return;
      }
    }

    long endNanos = System.nanoTime();

    double totalUs = (endNanos - startNanos) / 1000.0;
    double perIterUs = totalUs / iterations;
    double perPacketUs = perIterUs / numPackets;
    double throughputKbps = (input.length * 8.0 * 1000.0) / perIterUs;

    System.out.printf(
        "%-20s %8.2f \u00b5s/iter  %6.2f \u00b5s/pkt  %8.1f Kbps  (%d pkts)%n",
        config.name, perIterUs, perPacketUs, throughputKbps, numPackets);
  }

  /**
   * Main entry point for benchmarks.
   *
   * @param args command line arguments (optional: iteration count)
   */
  public static void main(String[] args) {
    int iterations = DEFAULT_ITERATIONS;

    if (args.length >= 1) {
      try {
        iterations = Integer.parseInt(args[0]);
        if (iterations <= 0) {
          iterations = DEFAULT_ITERATIONS;
        }
      } catch (NumberFormatException e) {
        iterations = DEFAULT_ITERATIONS;
      }
    }

    System.out.println("CCSDS 124.0-B-1 Benchmarks (Java Implementation)");
    System.out.println("=========================================");
    System.out.printf("Iterations: %d%n", iterations);
    System.out.printf("Packet size: %d bits (%d bytes)%n%n", PACKET_SIZE_BITS, PACKET_SIZE_BYTES);

    System.out.printf(
        "%-20s %14s  %13s  %12s  %s%n", "Test", "Time", "Per-Packet", "Throughput", "Packets");
    System.out.printf(
        "%-20s %14s  %13s  %12s  %s%n", "----", "----", "----------", "----------", "-------");

    // Compression benchmarks
    System.out.println("\nCompression:");
    for (BenchConfig config : BENCHMARKS) {
      benchCompress(config, iterations);
    }

    // Decompression benchmarks
    System.out.println("\nDecompression:");
    for (BenchConfig config : BENCHMARKS) {
      benchDecompress(config, iterations);
    }

    System.out.println("\nNote: Desktop performance differs from embedded targets.");
    System.out.println("Use these results for relative comparisons only.");
  }
}

/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

/**
 * Benchmark for CCSDS 124.0-B-1 compression/decompression.
 *
 * <p>Runs multiple iterations with JVM warmup to get accurate timing.
 */
public class Benchmark {

  private static final int PACKET_SIZE = 90;
  private static final int PT = 20;
  private static final int FT = 50;
  private static final int RT = 100;
  private static final int ROBUSTNESS = 2;
  private static final int WARMUP_ITERATIONS = 50;

  /**
   * Runs the benchmark.
   *
   * @param args command line arguments
   * @throws Exception if an error occurs
   */
  public static void main(String[] args) throws Exception {
    int iterations = 200;
    if (args.length > 0) {
      iterations = Integer.parseInt(args[0]);
    }

    String vectorDir = "../../test-vectors/input";
    String[] vectors = {"simple", "housekeeping", "venus-express", "hiro"};

    System.out.println("CCSDS 124.0-B-1 Benchmarks (Java Implementation)");
    System.out.println("=========================================");
    System.out.println("Iterations: " + iterations + " (warmup: " + WARMUP_ITERATIONS + ")");
    System.out.println("Packet size: " + (PACKET_SIZE * 8) + " bits (" + PACKET_SIZE + " bytes)");
    System.out.println();
    System.out.printf(
        "%-25s %12s %14s %14s  %s%n", "Test", "Time", "Per-Packet", "Throughput", "Packets");
    System.out.printf(
        "%-25s %12s %14s %14s  %s%n", "----", "----", "----------", "----------", "-------");
    System.out.println();
    System.out.println("Compression:");

    for (String vector : vectors) {
      Path inputPath = Paths.get(vectorDir, vector + ".bin");
      if (!Files.exists(inputPath)) {
        continue;
      }
      benchmarkCompression(inputPath, vector, iterations);
    }

    System.out.println();
    System.out.println("Decompression:");

    for (String vector : vectors) {
      Path inputPath = Paths.get(vectorDir, vector + ".bin");
      if (!Files.exists(inputPath)) {
        continue;
      }
      benchmarkDecompression(inputPath, vector, iterations);
    }
  }

  private static void benchmarkCompression(Path inputPath, String name, int iterations)
      throws Exception {
    byte[] inputData = Files.readAllBytes(inputPath);
    int numPackets = inputData.length / PACKET_SIZE;

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
      Ccsds124.compress(inputData, PACKET_SIZE, ROBUSTNESS, PT, FT, RT);
    }

    // Timed runs
    long totalNanos = 0;
    for (int i = 0; i < iterations; i++) {
      long start = System.nanoTime();
      Ccsds124.compress(inputData, PACKET_SIZE, ROBUSTNESS, PT, FT, RT);
      totalNanos += System.nanoTime() - start;
    }

    double avgMicros = (totalNanos / iterations) / 1000.0;
    double perPacketMicros = avgMicros / numPackets;
    double throughputKbps = (numPackets * PACKET_SIZE * 8.0) / avgMicros * 1000.0;

    System.out.printf(
        "%-25s %9.2f µs/iter %7.2f µs/pkt %10.1f Kbps  (%d pkts)%n",
        name, avgMicros, perPacketMicros, throughputKbps, numPackets);
  }

  private static void benchmarkDecompression(Path inputPath, String name, int iterations)
      throws Exception {
    byte[] inputData = Files.readAllBytes(inputPath);
    int numPackets = inputData.length / PACKET_SIZE;

    // Compress first to get compressed data
    byte[] compressed = Ccsds124.compress(inputData, PACKET_SIZE, ROBUSTNESS, PT, FT, RT);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
      Ccsds124.decompress(compressed, PACKET_SIZE, ROBUSTNESS);
    }

    // Timed runs
    long totalNanos = 0;
    for (int i = 0; i < iterations; i++) {
      long start = System.nanoTime();
      Ccsds124.decompress(compressed, PACKET_SIZE, ROBUSTNESS);
      totalNanos += System.nanoTime() - start;
    }

    double avgMicros = (totalNanos / iterations) / 1000.0;
    double perPacketMicros = avgMicros / numPackets;
    double throughputKbps = (numPackets * PACKET_SIZE * 8.0) / avgMicros * 1000.0;

    System.out.printf(
        "%-25s %9.2f µs/iter %7.2f µs/pkt %10.1f Kbps  (%d pkts)%n",
        name, avgMicros, perPacketMicros, throughputKbps, numPackets);
  }
}

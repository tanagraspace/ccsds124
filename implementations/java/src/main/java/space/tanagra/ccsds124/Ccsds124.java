/*
 * Copyright (c) 2025 Tanagra Space
 * SPDX-License-Identifier: MIT
 */

package space.tanagra.ccsds124;

/**
 * CCSDS 124.0-B-1 lossless compression algorithm for satellite housekeeping telemetry.
 *
 * <p>This class provides the high-level API for compressing and decompressing data using the CCSDS
 * 124.0-B-1 algorithm.
 *
 * <p>Example usage:
 *
 * <pre>{@code
 * // Compress data
 * byte[] compressed = Ccsds124.compress(input, 90, 2, 20, 50, 100);
 *
 * // Decompress data
 * byte[] decompressed = Ccsds124.decompress(compressed, 90, 2);
 * }</pre>
 *
 * @see <a href="https://ccsds.org/Pubs/124x0b1.pdf">CCSDS 124.0-B-1</a>
 */
public final class Ccsds124 {

  /** Library version. */
  public static final String VERSION = "1.0.0";

  /** Maximum packet length in bits (CCSDS limit). */
  public static final int MAX_PACKET_LENGTH = 65535;

  /** Maximum packet length in bytes. */
  public static final int MAX_PACKET_BYTES = (MAX_PACKET_LENGTH + 7) / 8;

  /** Maximum robustness value. */
  public static final int MAX_ROBUSTNESS = 7;

  private Ccsds124() {
    // Utility class - prevent instantiation
  }

  /**
   * Returns the library version string.
   *
   * @return version string in format "major.minor.patch"
   */
  public static String version() {
    return VERSION;
  }

  /**
   * Compresses data using the CCSDS 124.0-B-1 algorithm.
   *
   * @param data input data to compress (multiple packets concatenated)
   * @param packetSize size of each packet in bytes
   * @param robustness robustness parameter R (0-7)
   * @param ptLimit Pt parameter limit
   * @param ftLimit Ft parameter limit
   * @param rtLimit Rt parameter limit
   * @return compressed data
   * @throws Ccsds124Exception if compression fails
   */
  public static byte[] compress(
      byte[] data, int packetSize, int robustness, int ptLimit, int ftLimit, int rtLimit)
      throws Ccsds124Exception {
    if (data == null) {
      throw new Ccsds124Exception("Input data cannot be null");
    }
    if (packetSize <= 0 || packetSize > MAX_PACKET_BYTES) {
      throw new Ccsds124Exception("Invalid packet size: " + packetSize);
    }
    if (robustness < 0 || robustness > MAX_ROBUSTNESS) {
      throw new Ccsds124Exception("Invalid robustness: " + robustness);
    }
    if (data.length % packetSize != 0) {
      throw new Ccsds124Exception(
          "Data length must be multiple of packet size: " + data.length + " % " + packetSize);
    }

    int numPackets = data.length / packetSize;
    int packetBits = packetSize * 8;

    Compressor compressor = new Compressor(packetBits, null, robustness, ptLimit, ftLimit, rtLimit);
    java.io.ByteArrayOutputStream output = new java.io.ByteArrayOutputStream();

    for (int i = 0; i < numPackets; i++) {
      byte[] packetData = new byte[packetSize];
      System.arraycopy(data, i * packetSize, packetData, 0, packetSize);
      BitVector input = BitVector.fromBytes(packetData, packetBits);

      BitBuffer packetOutput = compressor.compressPacket(input);
      // Convert each packet to bytes with byte-boundary padding (matches C implementation)
      byte[] packetBytes = packetOutput.toBytes();
      output.write(packetBytes, 0, packetBytes.length);
    }

    return output.toByteArray();
  }

  /**
   * Decompresses data using the CCSDS 124.0-B-1 algorithm.
   *
   * @param data compressed data to decompress
   * @param packetSize size of each decompressed packet in bytes
   * @param robustness robustness parameter R (0-7)
   * @return decompressed data
   * @throws Ccsds124Exception if decompression fails
   */
  public static byte[] decompress(byte[] data, int packetSize, int robustness)
      throws Ccsds124Exception {
    if (data == null) {
      throw new Ccsds124Exception("Input data cannot be null");
    }
    if (packetSize <= 0 || packetSize > MAX_PACKET_BYTES) {
      throw new Ccsds124Exception("Invalid packet size: " + packetSize);
    }
    if (robustness < 0 || robustness > MAX_ROBUSTNESS) {
      throw new Ccsds124Exception("Invalid robustness: " + robustness);
    }

    int packetBits = packetSize * 8;
    Decompressor decompressor = new Decompressor(packetBits, null, robustness);
    BitReader reader = new BitReader(data, data.length * 8);

    java.io.ByteArrayOutputStream outputStream = new java.io.ByteArrayOutputStream();

    while (reader.remaining() > 0) {
      try {
        BitVector output = decompressor.decompressPacket(reader);
        byte[] packetBytes = output.toBytes();
        outputStream.write(packetBytes, 0, packetSize);

        // Align to byte boundary for next packet (matches C implementation)
        reader.alignByte();
      } catch (Ccsds124Exception e) {
        // End of stream or incomplete packet
        break;
      }
    }

    return outputStream.toByteArray();
  }
}

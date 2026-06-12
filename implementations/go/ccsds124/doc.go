// Package ccsds124 implements the CCSDS 124.0-B-1 lossless
// compression algorithm for fixed-length housekeeping data.
//
// CCSDS 124.0-B-1 is designed for compressing telemetry data from spacecraft,
// where consecutive packets typically have many bits that remain unchanged
// or follow predictable patterns.
//
// Basic usage:
//
//	// Compress data
//	compressed, err := ccsds124.Compress(data, packetSize, robustness, pt, ft, rt)
//	if err != nil {
//	    log.Fatal(err)
//	}
//
//	// Decompress data
//	decompressed, err := ccsds124.Decompress(compressed, packetSize, robustness)
//	if err != nil {
//	    log.Fatal(err)
//	}
//
// For more information about the CCSDS 124.0-B-1 algorithm, see:
// https://ccsds.org/Pubs/124x0b1.pdf
package ccsds124

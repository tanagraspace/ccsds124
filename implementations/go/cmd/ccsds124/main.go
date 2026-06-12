// Package main provides the CCSDS 124.0-B-1 command line interface.
//
// CCSDS 124.0-B-1 is a lossless compression algorithm specified in CCSDS 124.0-B-1.
// This CLI provides compress and decompress functionality similar to gzip.
package main

import (
	"fmt"
	"os"
	"strconv"
	"strings"

	"github.com/tanagraspace/ccsds124/implementations/go/ccsds124"
)

const banner = `  ____ ____ ____  ____  ____    _ ____  _  _
 / ___/ ___/ ___||  _ \/ ___|  / |___ \| || |
| |  | |   \___ \| | | \___ \  | | __) | || |_
| |__| |___ ___) | |_| |___) | | |/ __/|__   _|
 \____\____|____/|____/|____/  |_|_____|  |_|

         by  T A N A G R A  S P A C E`

func printVersion() {
	fmt.Printf("ccsds124 %s (Go)\n", ccsds124.Version)
}

func printHelp(progName string) {
	fmt.Print(banner)
	fmt.Println()
	fmt.Printf("CCSDS 124.0-B-1 Lossless Compression (v%s)\n", ccsds124.Version)
	fmt.Println("=================================================")
	fmt.Println()
	fmt.Println("References:")
	fmt.Println("  CCSDS 124.0-B-1: https://ccsds.org/Pubs/124x0b1.pdf")
	fmt.Println("  Documentation: https://tanagraspace.com/ccsds124")
	fmt.Println()
	fmt.Println("Citation:")
	fmt.Println("  D. Evans, G. Labreche, D. Marszk, S. Bammens, M. Hernandez-Cabronero,")
	fmt.Println("  V. Zelenevskiy, V. Shiradhonkar, M. Starcik, and M. Henkel. 2022.")
	fmt.Println("  \"Implementing the New CCSDS Housekeeping Data Compression Standard")
	fmt.Println("  124.0-B-1 (based on POCKET+) on OPS-SAT-1,\" Proceedings of the")
	fmt.Println("  Small Satellite Conference, Communications, SSC22-XII-03.")
	fmt.Println("  https://digitalcommons.usu.edu/smallsat/2022/all2022/133/")
	fmt.Println()
	fmt.Println("Usage:")
	fmt.Printf("  %s <input> <packet_size> <pt> <ft> <rt> <robustness>\n", progName)
	fmt.Printf("  %s -d <input.pkt> <packet_size> <robustness>\n", progName)
	fmt.Println()
	fmt.Println("Options:")
	fmt.Println("  -d             Decompress (default is compress)")
	fmt.Println("  -h, --help     Show this help message")
	fmt.Println("  -v, --version  Show version information")
	fmt.Println()
	fmt.Println("Compress arguments:")
	fmt.Println("  input          Input file to compress")
	fmt.Println("  packet_size    Packet size in bytes (e.g., 90)")
	fmt.Println("  pt             New mask period (e.g., 10, 20)")
	fmt.Println("  ft             Send mask period (e.g., 20, 50)")
	fmt.Println("  rt             Uncompressed period (e.g., 50, 100)")
	fmt.Println("  robustness     Robustness level 0-7 (e.g., 1, 2)")
	fmt.Println()
	fmt.Println("Decompress arguments:")
	fmt.Println("  input.pkt      Compressed input file")
	fmt.Println("  packet_size    Original packet size in bytes")
	fmt.Println("  robustness     Robustness level (must match compression)")
	fmt.Println()
	fmt.Println("Output:")
	fmt.Println("  Compress:   <input>.pkt")
	fmt.Println("  Decompress: <input>.depkt (or <base>.depkt if input ends in .pkt)")
	fmt.Println()
	fmt.Println("Examples:")
	fmt.Printf("  %s data.bin 90 10 20 50 1        # compress\n", progName)
	fmt.Printf("  %s -d data.bin.pkt 90 1          # decompress\n", progName)
	fmt.Println()
}

func makeDecompressFilename(input string) string {
	if strings.HasSuffix(input, ".pkt") {
		return strings.TrimSuffix(input, ".pkt") + ".depkt"
	}
	return input + ".depkt"
}

func doCompress(inputPath string, packetSize, pt, ft, rt, robustness int) int {
	// Read input file
	inputData, err := os.ReadFile(inputPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: Cannot open input file: %s\n", inputPath)
		return 1
	}

	if len(inputData) == 0 {
		fmt.Fprintln(os.Stderr, "Error: Input file is empty")
		return 1
	}

	if len(inputData)%packetSize != 0 {
		fmt.Fprintf(os.Stderr, "Error: Input size (%d) not divisible by packet size (%d)\n",
			len(inputData), packetSize)
		return 1
	}

	// Compress
	outputData, err := ccsds124.Compress(inputData, packetSize, robustness, pt, ft, rt)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: Compression failed: %v\n", err)
		return 1
	}

	// Write output
	outputPath := inputPath + ".pkt"
	err = os.WriteFile(outputPath, outputData, 0644)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: Cannot write output file: %s\n", outputPath)
		return 1
	}

	// Print summary
	numPackets := len(inputData) / packetSize
	ratio := float64(len(inputData)) / float64(len(outputData))
	fmt.Printf("Input:       %s (%d bytes, %d packets)\n", inputPath, len(inputData), numPackets)
	fmt.Printf("Output:      %s (%d bytes)\n", outputPath, len(outputData))
	fmt.Printf("Ratio:       %.2fx\n", ratio)
	fmt.Printf("Parameters:  R=%d, pt=%d, ft=%d, rt=%d\n", robustness, pt, ft, rt)

	return 0
}

func doDecompress(inputPath string, packetSize, robustness int) int {
	// Read input file
	inputData, err := os.ReadFile(inputPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: Cannot open input file: %s\n", inputPath)
		return 1
	}

	if len(inputData) == 0 {
		fmt.Fprintln(os.Stderr, "Error: Input file is empty")
		return 1
	}

	// Decompress
	outputData, err := ccsds124.Decompress(inputData, packetSize, robustness)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: Decompression failed: %v\n", err)
		return 1
	}

	// Write output
	outputPath := makeDecompressFilename(inputPath)
	err = os.WriteFile(outputPath, outputData, 0644)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: Cannot write output file: %s\n", outputPath)
		return 1
	}

	// Print summary
	numPackets := len(outputData) / packetSize
	expansion := float64(len(outputData)) / float64(len(inputData))
	fmt.Printf("Input:       %s (%d bytes)\n", inputPath, len(inputData))
	fmt.Printf("Output:      %s (%d bytes, %d packets)\n", outputPath, len(outputData), numPackets)
	fmt.Printf("Expansion:   %.2fx\n", expansion)
	fmt.Printf("Parameters:  packet_size=%d, R=%d\n", packetSize, robustness)

	return 0
}

func main() {
	args := os.Args
	progName := args[0]

	// Check for help flag
	if len(args) < 2 || args[1] == "-h" || args[1] == "--help" {
		printHelp(progName)
		if len(args) < 2 {
			os.Exit(1)
		}
		os.Exit(0)
	}

	// Check for version flag
	if args[1] == "-v" || args[1] == "--version" {
		printVersion()
		os.Exit(0)
	}

	// Check for decompress flag
	decompressMode := false
	argOffset := 1
	if args[1] == "-d" {
		decompressMode = true
		argOffset = 2
	}

	if decompressMode {
		// Decompress mode: -d <input.pkt> <packet_size> <robustness>
		if len(args) != 5 {
			fmt.Fprintln(os.Stderr, "Error: Decompress requires 3 arguments after -d")
			fmt.Fprintf(os.Stderr, "Usage: %s -d <input.pkt> <packet_size> <robustness>\n", progName)
			os.Exit(1)
		}

		inputPath := args[argOffset]
		packetSize, err := strconv.Atoi(args[argOffset+1])
		if err != nil || packetSize <= 0 || packetSize > 8192 {
			fmt.Fprintln(os.Stderr, "Error: packet_size must be 1-8192 bytes")
			os.Exit(1)
		}

		robustness, err := strconv.Atoi(args[argOffset+2])
		if err != nil || robustness < 0 || robustness > 7 {
			fmt.Fprintln(os.Stderr, "Error: robustness must be 0-7")
			os.Exit(1)
		}

		os.Exit(doDecompress(inputPath, packetSize, robustness))
	} else {
		// Compress mode: <input> <packet_size> <pt> <ft> <rt> <robustness>
		if len(args) != 7 {
			fmt.Fprintln(os.Stderr, "Error: Compress requires 6 arguments")
			fmt.Fprintf(os.Stderr, "Usage: %s <input> <packet_size> <pt> <ft> <rt> <robustness>\n", progName)
			os.Exit(1)
		}

		inputPath := args[1]
		packetSize, err := strconv.Atoi(args[2])
		if err != nil || packetSize <= 0 || packetSize > 8192 {
			fmt.Fprintln(os.Stderr, "Error: packet_size must be 1-8192 bytes")
			os.Exit(1)
		}

		pt, err := strconv.Atoi(args[3])
		if err != nil || pt <= 0 {
			fmt.Fprintln(os.Stderr, "Error: pt must be positive")
			os.Exit(1)
		}

		ft, err := strconv.Atoi(args[4])
		if err != nil || ft <= 0 {
			fmt.Fprintln(os.Stderr, "Error: ft must be positive")
			os.Exit(1)
		}

		rt, err := strconv.Atoi(args[5])
		if err != nil || rt <= 0 {
			fmt.Fprintln(os.Stderr, "Error: rt must be positive")
			os.Exit(1)
		}

		robustness, err := strconv.Atoi(args[6])
		if err != nil || robustness < 0 || robustness > 7 {
			fmt.Fprintln(os.Stderr, "Error: robustness must be 0-7")
			os.Exit(1)
		}

		os.Exit(doCompress(inputPath, packetSize, pt, ft, rt, robustness))
	}
}

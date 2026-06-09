# POCKET+ Go Implementation

[![Build](https://github.com/tanagraspace/pocket-plus/actions/workflows/go-build.yml/badge.svg)](https://github.com/tanagraspace/pocket-plus/actions/workflows/go-build.yml)
[![Coverage](assets/coverage.svg)](https://tanagraspace.com/pocket-plus/go/coverage/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A Go implementation of the [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf) POCKET+ lossless compression algorithm for fixed-length housekeeping data.

## Citation

If POCKET+ contributes to your research, please cite:

> D. Evans, G. Labrèche, D. Marszk, S. Bammens, M. Hernandez-Cabronero, V. Zelenevskiy, V. Shiradhonkar, M. Starcik, and M. Henkel. 2022. "Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1," *Proceedings of the Small Satellite Conference*, Communications, SSC22-XII-03. https://digitalcommons.usu.edu/smallsat/2022/all2022/133/

<details>
<summary>BibTeX</summary>

```bibtex
@inproceedings{evans2022pocketplus,
  author    = {Evans, David and Labrèche, Georges and Marszk, Dominik and Bammens, Samuel and Hernandez-Cabronero, Miguel and Zelenevskiy, Vladimir and Shiradhonkar, Vasundhara and Starcik, Mario and Henkel, Maximilian},
  title     = {Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1},
  booktitle = {Proceedings of the Small Satellite Conference},
  year      = {2022},
  note      = {SSC22-XII-03},
  url       = {https://digitalcommons.usu.edu/smallsat/2022/all2022/133/}
}
```

</details>

## Building

```bash
make build         # Build library
make cli           # Build CLI tool
make test          # Run all tests
make test-race     # Run tests with race detection
make coverage      # Run tests with coverage report
make coverage-html # Generate HTML coverage report
make docs          # Generate all documentation
make clean         # Clean build artifacts
```

### Docker

```bash
docker-compose run --rm go                    # Build, test, coverage, docs
docker-compose run --rm go make test          # Run tests only
docker-compose run --rm --build go            # Rebuild after changes
```

## CLI

```bash
# Compress
./build/pocketplus compress -i <input> -o <output> -s <packet_size> -r <robustness> \
    --pt <pt_limit> --ft <ft_limit> --rt <rt_limit>

# Decompress
./build/pocketplus decompress -i <input> -o <output> -s <packet_size> -r <robustness>
```

**Example:**
```bash
./build/pocketplus compress -i data.bin -o data.pkt -s 90 -r 2 --pt 20 --ft 50 --rt 100
./build/pocketplus decompress -i data.pkt -o data.restored -s 90 -r 2
```

Run `./build/pocketplus --help` for full usage.

## Library Usage

```go
package main

import (
    "fmt"
    "github.com/tanagraspace/pocket-plus/implementations/go/pocketplus"
)

func main() {
    // Compress
    compressed, numBits, err := pocketplus.Compress(
        inputData,  // []byte: multiple fixed-length packets
        90,         // packet size in bytes
        2,          // robustness (0-7)
        20, 50, 100, // Pt, Ft, Rt limits
    )

    // Decompress
    decompressed, err := pocketplus.Decompress(
        compressed,
        numBits,
        90,  // packet size
        2,   // robustness
    )
}
```

## Design

- **Zero dependencies** - Go standard library only
- **Idiomatic Go** - Proper error handling, interfaces, documentation
- **Memory efficient** - Pre-allocated buffers, streaming decompression API
- **Optimized** - Word-level bit operations, batch bit appending

## File Structure

```
implementations/go/
├── pocketplus/
│   ├── doc.go            # Package documentation
│   ├── bitvector.go      # Fixed-length bit vectors
│   ├── bitbuffer.go      # Variable-length output buffer
│   ├── bitreader.go      # Sequential bit reading
│   ├── encode.go         # COUNT, RLE, BE encoding
│   ├── decode.go         # COUNT, RLE decoding
│   ├── mask.go           # Mask update logic
│   ├── compress.go       # High-level Compress API
│   ├── compressor.go     # Compression algorithm
│   ├── decompress.go     # High-level Decompress API
│   ├── decompressor.go   # Decompression algorithm
│   └── *_test.go         # Unit tests
├── cmd/pocketplus/       # CLI tool
├── scripts/              # Documentation generators
└── Makefile
```

## API

### High-Level

- `Compress()` / `Decompress()` - Compress/decompress entire buffer
- `NewCompressor()` / `NewDecompressor()` - Create stateful instances

### Low-Level

- `CompressPacket()` / `DecompressPacket()` - Single packet operations
- `CountEncode()` / `CountDecode()` - Counter encoding (Eq. 9)
- `RLEEncode()` / `RLEDecode()` - Run-length encoding (Eq. 10)
- `BitExtract()` / `BitInsert()` - Bit extraction (Eq. 11)

### Streaming Decompression

```go
// Iterator pattern
iter := decomp.NewPacketIterator(data, numBits)
for packet := iter.Next(); packet != nil; packet = iter.Next() {
    process(packet)
}

// Channel pattern
for packet := range decomp.StreamPackets(data, numBits) {
    process(packet)
}
```

## References

- [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf)
- [POCKET+ on OPS-SAT-1 (SmallSat 2022)](https://digitalcommons.usu.edu/smallsat/2022/all2022/133/)

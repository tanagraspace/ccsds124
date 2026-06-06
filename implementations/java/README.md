# POCKET+ Java Implementation

[![Build](https://github.com/tanagraspace/pocket-plus/actions/workflows/java-build.yml/badge.svg)](https://github.com/tanagraspace/pocket-plus/actions/workflows/java-build.yml)
[![Coverage](assets/coverage.svg)](https://tanagraspace.com/pocket-plus/java/coverage/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Java implementation of the [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf) POCKET+ lossless compression algorithm for satellite housekeeping telemetry.

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

## Requirements

- Java 11 or later
- Maven 3.6 or later

## Building

```bash
# Build JAR
make build

# Run tests
make test

# Run all quality checks (format, checkstyle, spotbugs, tests)
make check

# Generate coverage report
make coverage

# Generate API documentation
make docs
```

### Docker

```bash
# Build and run tests in container
docker build -t pocketplus-java .
docker run --rm pocketplus-java

# Or use docker-compose from repository root
docker-compose run --rm java
```

## Usage

### Command Line

```bash
# Compress
java -jar target/pocketplus-1.0.0.jar input.bin 90 20 50 100 2

# Decompress
java -jar target/pocketplus-1.0.0.jar -d input.bin.pkt 90 2

# Show version
java -jar target/pocketplus-1.0.0.jar --version
```

### Library API

```java
import space.tanagra.pocketplus.PocketPlus;

// Compress data
byte[] compressed = PocketPlus.compress(
    inputData,    // byte array of concatenated packets
    90,           // packet size in bytes
    2,            // robustness (0-7)
    20,           // pt limit
    50,           // ft limit
    100           // rt limit
);

// Decompress data
byte[] decompressed = PocketPlus.decompress(
    compressed,   // compressed data
    90,           // packet size in bytes
    2             // robustness (0-7)
);

// Check version
String version = PocketPlus.version();
```

### Parameters

| Parameter | Description | Range |
|-----------|-------------|-------|
| `packet_size` | Size of each packet in bytes | 1-8191 |
| `robustness` | Robustness parameter R | 0-7 |
| `pt` | New mask period limit | > 0 |
| `ft` | Full transmit period limit | > 0 |
| `rt` | Reset period limit | > 0 |

## Design

- **Zero runtime dependencies** - Only requires Java 11+
- **Clean OOP design** - Proper interfaces, classes, and encapsulation
- **Comprehensive Javadoc** - All public APIs documented
- **High test coverage** - Unit tests and reference vector validation
- **CLI tool** - Command-line interface for compress/decompress operations

## Project Structure

```
implementations/java/
├── src/main/java/space/tanagra/pocketplus/
│   ├── BitVector.java       # Fixed-length bit vectors
│   ├── BitBuffer.java       # Variable-length output buffer
│   ├── BitReader.java       # Sequential bit reading
│   ├── Encoder.java         # COUNT, RLE, BE encoding
│   ├── Decoder.java         # COUNT, RLE decoding
│   ├── MaskOperations.java  # Mask/build vector operations
│   ├── Compressor.java      # Compression algorithm
│   ├── Decompressor.java    # Decompression algorithm
│   ├── PocketPlus.java      # High-level API
│   ├── PocketException.java # Custom exception
│   └── cli/Main.java        # CLI tool
├── src/test/java/           # Unit tests
├── pom.xml                  # Maven build
├── checkstyle.xml           # Code style rules
├── Makefile                 # Build targets
└── Dockerfile               # Container build
```

## API

### High-Level

- `PocketPlus.compress()` / `PocketPlus.decompress()` - Compress/decompress entire buffer
- `PocketPlus.version()` - Library version

### Low-Level

- `Compressor.compressPacket()` / `Decompressor.decompressPacket()` - Single packet
- `Encoder` / `Decoder` - COUNT, RLE, BE encoding/decoding (Eq. 9-11)
- `BitVector` / `BitBuffer` / `BitReader` - Bit-level primitives
- `MaskOperations` - Mask/build vector updates

## Test Vectors

The implementation is validated against reference test vectors:

| Vector | Packets | Robustness | Expected Ratio |
|--------|---------|------------|----------------|
| simple | 100 | R=1 | 14.04x |
| housekeeping | 10,000 | R=2 | 4.03x |
| edge-cases | 500 | R=1 | 4.44x |
| venus-express | 151,200 | R=2 | 2.31x |
| hiro | 100 | R=7 | 5.87x |

## References

- [CCSDS 124.0-B-1 Standard](https://ccsds.org/Pubs/124x0b1.pdf)
- [POCKET+ on OPS-SAT-1](https://digitalcommons.usu.edu/smallsat/2022/all2022/133/)

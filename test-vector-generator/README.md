# CCSDS 124.0-B-1 Test Vector Generator

Deterministic, containerized test vector generation for CCSDS 124.0-B-1 compression algorithm using the ESA C reference implementation.

## Overview

This directory contains a complete Docker-based system for generating test vectors that validate CCSDS 124.0-B-1 implementations across different programming languages (C, C++, Python, Go, Rust, Java). The generator ensures:

- **Deterministic builds** - Pinned dependencies (Alpine 3.18, GCC 12.2.1, Python 3.11)
- **Reproducible results** - Fixed seeds for all random generation
- **Automated verification** - Compress → decompress → verify pipeline
- **Multiple test scenarios** - Real spacecraft data + synthetic edge cases

## Quick Start

```bash
# Generate all test vectors
make generate

# Quick test with simple vector
make test

# Generate specific test vector
make generate-simple
make generate-hiro
make generate-housekeeping
make generate-edge-cases
make generate-venus-express

# Copy to repository test-vectors/
make copy-to-repo
```

## Test Vectors

### Compression Results

| Test Vector | Input Size | Compressed | Ratio | Description |
|-------------|-----------|------------|-------|-------------|
| **simple** | 9 KB (100 packets) | 641 bytes | **14.04×** | High compressibility patterns |
| **hiro** | 9 KB (100 packets) | 1,533 bytes | **5.87×** | High robustness (R=7) |
| **housekeeping** | 900 KB (10,000 packets) | 223 KB | **4.03×** | Realistic telemetry simulation |
| **edge-cases** | 45 KB (500 packets) | 10.1 KB | **4.44×** | Mixed stable/changing sections |
| **venus-express** | 13.6 MB (151,200 packets) | 5.9 MB | **2.31×** | Real spacecraft data |

### 1. simple (Synthetic - High Compressibility)
- **Size**: 100 packets (9 KB)
- **Compression**: 14.04× (9 KB → 641 bytes)
- **Parameters**: pt=10, ft=20, rt=50, robustness=1
- **Patterns**:
  - Repeating sequences (0x08, 0xD4, 0xF1, 0xAB)
  - Incrementing counters
  - Large blocks of stable zeros
  - Slowly changing values (every 10 packets)
  - Simple repeating sequences (0x01, 0x02, 0x03, 0x04)
- **Purpose**: Quick validation and testing; demonstrates excellent compression with highly predictable data

### 2. hiro (Synthetic - High Robustness)
- **Size**: 100 packets (9 KB)
- **Compression**: 5.87× (9 KB → 1,533 bytes)
- **Parameters**: pt=10, ft=20, rt=50, robustness=7
- **Patterns**: Same as simple
- **Purpose**: Tests high robustness level (R=7) which increases compressed output size for better error resilience

### 3. housekeeping (Synthetic - Realistic)
- **Size**: 10,000 packets (900 KB)
- **Compression**: 4.03× (900 KB → 223 KB)
- **Parameters**: pt=20, ft=50, rt=100, robustness=2
- **Patterns**: Simulates realistic spacecraft housekeeping telemetry
  - **CCSDS headers**: Packet version, APID 1028, sequence counts
  - **Timestamps**: Incrementing by 1000ms per packet
  - **Temperature sensors**: Integer-scaled values (×100), changing every 50 packets
  - **Voltage monitors**: Integer-scaled values (×1000), changing every 100 packets
  - **Current sensors**: Integer-scaled values (×1000), changing every 25 packets
  - **Gyroscopes**: Signed integer values (×10000), changing every 10 packets
  - **Accelerometers**: Signed integer values (×1000), changing every 20 packets
  - **Counters**: Command counter, telemetry counter, uptime in milliseconds
  - **Status flags**: Occasional bit flips
- **Purpose**: Represents realistic spacecraft behavior with slowly changing sensor values; validates compression on typical telemetry patterns

### 4. edge-cases (Synthetic - Mixed Patterns)
- **Size**: 500 packets (45 KB)
- **Compression**: 4.44× (45 KB → 10.1 KB)
- **Parameters**: pt=10, ft=20, rt=50, robustness=1
- **Patterns**: Tests partial packet changes
  - **Bytes 0-29**: Completely stable (always 0x00)
  - **Bytes 30-59**: Step-wise incrementing values (0→95, +5 every 25 packets)
  - **Bytes 60-89**: Completely stable (always 0xFF)
- **Purpose**: Tests algorithm efficiency when only a portion of each packet changes while the rest remains constant; demonstrates compression with gradual transitions

### 5. venus-express (Real Data)
- **Source**: Actual Venus Express spacecraft ADCS (Attitude Determination and Control System) telemetry
- **Size**: 13.6 MB (151,200 packets × 90 bytes)
- **Compression**: 2.31× (13.6 MB → 5.9 MB)
- **Parameters**: pt=20, ft=50, rt=100, robustness=2
- **Duration**: 1 week's worth of packet ID 1028
- **MD5**: ac0ce25d660efa5ee18f92b71aa9a85d
- **Purpose**: Gold standard validation with proven real-world spacecraft data; represents actual compression performance on operational telemetry

## Directory Structure

```
test-vector-generator/
├── Dockerfile                 # Multi-stage build (builder + generator)
├── docker-compose.yml         # Orchestration
├── Makefile                   # Convenience commands
├── README.md                  # This file
│
├── c-reference/               # ESA reference implementation
│   ├── pocket_compress.c      # Compression implementation (48 KB)
│   ├── pocket_decompress.c    # Decompression implementation (30 KB)
│   ├── Makefile               # Build configuration
│   ├── README.md              # Original ESA documentation
│   ├── PROVENANCE.md          # Source provenance & checksums
│   └── .gitignore             # Ignore compiled binaries
│
├── configs/                   # YAML configurations
│   ├── venus-express.yaml     # Real data config
│   ├── simple.yaml            # Simple synthetic config
│   ├── housekeeping.yaml      # Realistic synthetic config
│   └── edge-cases.yaml        # Edge cases config
│
├── input-generators/          # Python generators for synthetic data
│   ├── common.py              # Shared utilities
│   ├── generate.py            # Unified generator with fault injection
│   ├── generate_simple.py     # Simple patterns
│   ├── generate_housekeeping.py  # Realistic telemetry
│   ├── generate_edge-cases.py    # Mixed stable/changing patterns
│   └── requirements.txt       # numpy, pyyaml
│
├── scripts/                   # Orchestration scripts
│   ├── generate-all.sh        # Main generation script
│   ├── verify-vector.sh       # Compress/decompress/verify
│   ├── validate_ccsds_style.py   # CCSDS-style fault injection validator
│   └── copy-to-repo.sh        # Copy to test-vectors/
│
└── output/                    # Generated test vectors (gitignored)
    └── vectors/
        ├── simple/
        ├── housekeeping/
        ├── edge-cases/
        └── venus-express/
```

## How It Works

### Generation Pipeline

```
┌─────────────┐
│   Config    │
│  (YAML)     │
└──────┬──────┘
       │
       v
┌─────────────────────┐      ┌───────────────┐
│  Python Generator   │  →   │  Binary File  │
│  (synthetic only)   │      │  (.bin)       │
└─────────────────────┘      └───────┬───────┘
                                     │
                                     v
                              ┌─────────────────┐
                              │ C Compressor    │
                              │ pocket_compress │
                              └──────┬──────────┘
                                     │
                                     v
                              ┌──────────────┐
                              │ Compressed   │
                              │ (.pkt)       │
                              └──────┬───────┘
                                     │
                                     v
                              ┌───────────────────┐
                              │ C Decompressor    │
                              │ pocket_decompress │
                              └──────┬────────────┘
                                     │
                                     v
                              ┌──────────────┐
                              │ Decompressed │
                              │ (.depkt)     │
                              └──────┬───────┘
                                     │
                                     v
                              ┌──────────────┐
                              │ Verify MD5   │
                              │ (match?)     │
                              └──────┬───────┘
                                     │
                                     v
                              ┌──────────────┐
                              │  metadata    │
                              │  (.json)     │
                              └──────────────┘
```

### Docker Build Process

1. **Stage 1 (Builder)**:
   - Copy C source files from `c-reference/`
   - Compile with GCC 12.2.1 using static linking
   - Produces `pocket_compress` and `pocket_decompress` binaries

2. **Stage 2 (Generator)**:
   - Install Python 3.11 + numpy + pyyaml
   - Copy compiled binaries from builder
   - Copy Venus Express data from `../test-vectors/input/`
   - Copy generators, configs, scripts
   - Ready to generate test vectors

## Deterministic Guarantees

All outputs are deterministic and reproducible:

1. **Fixed Docker base**: Alpine Linux 3.18
2. **Pinned compiler**: GCC 12.2.1_git20220924-r10
3. **Pinned Python**: 3.11.9-r0
4. **Pinned packages**: numpy==1.24.3, pyyaml==6.0
5. **Static compilation**: `-static -no-pie` flags
6. **Fixed RNG seeds**: All generators use deterministic seeds
7. **Environment vars**: `PYTHONHASHSEED=0`, `LC_ALL=C`

## Configuration Format

Each test vector has a YAML configuration file:

```yaml
name: example
description: |
  Description of the test vector

input:
  type: synthetic  # or real_data
  packet_length: 90
  num_packets: 1000
  seed: 42  # For synthetic only

compression:
  packet_length: 90
  pt: 20   # Tracking period
  ft: 50   # Full mask transmission frequency
  rt: 100  # Full packet transmission frequency
  robustness: 2

expected:
  compression_ratio_min: 8.0
  compression_ratio_max: 10.0
  compression_ratio_typical: 9.0

output:
  dir: /workspace/output/vectors/example
  input_file: example.bin
  compressed_file: example.bin.pkt
  decompressed_file: example.bin.pkt.depkt
  metadata_file: metadata.json
```

## Output Metadata

Each test vector produces a `metadata.json`:

```json
{
  "name": "simple",
  "generated_at": "2025-12-02T20:19:00Z",
  "input": {
    "file": "simple.bin",
    "size": 9000,
    "md5": "2ec573b753ffe1defcd0085ac2b69c35",
    "type": "synthetic"
  },
  "compression": {
    "packet_length": 90,
    "parameters": {
      "pt": 10,
      "ft": 20,
      "rt": 50,
      "robustness": 1
    }
  },
  "output": {
    "compressed": {
      "file": "simple.bin.pkt",
      "size": 641,
      "md5": "..."
    },
    "decompressed": {
      "file": "simple.bin.pkt.depkt",
      "size": 9000,
      "md5": "2ec573b753ffe1defcd0085ac2b69c35"
    }
  },
  "results": {
    "compression_ratio": 14.04,
    "roundtrip_verified": true
  }
}
```

## Development

### Adding a New Test Vector

1. Create YAML config in `configs/new-vector.yaml`
2. If synthetic, create generator in `input-generators/generate_new_vector.py`
3. Update `scripts/generate-all.sh` to include new vector
4. Update this README

### Modifying Existing Generators

Python generators are in `input-generators/`. Common utilities are in `common.py`:

- `PacketGenerator` - Base class for all generators
- `write_value()` - Write integers to packets
- `write_float32()` - Write floats
- `write_random_bytes()` - Deterministic random data
- `calculate_crc16_ccitt()` - CRC checksums
- `calculate_md5()` - MD5 hashes

### Testing Changes

```bash
# Test with simple vector (fastest)
make test

# Generate all and verify
make generate
make verify

# Open shell for debugging
make shell
```

## Troubleshooting

### Build fails
```bash
# Clean and rebuild
make clean-all
make build
```

### Generation fails
```bash
# Check logs
docker-compose run --rm test-vector-generator

# Open shell to debug
make shell
```

### Verification fails
Check `output/vectors/*/metadata.json` for details:
```bash
cat output/vectors/simple/metadata.json | jq .results
```

## References

- **ESA Source**: Barebone C reference implementation provided by ESA
- **CCSDS 124.0-B-1**: Robust Compression of Fixed-Length Housekeeping Data. https://ccsds.org/Pubs/124x0b1.pdf
- **Implementation Paper**: Evans, D., et al. (2022). "Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1." 36th Annual Small Satellite Conference. https://digitalcommons.usu.edu/cgi/viewcontent.cgi?article=5292&context=smallsat
- **Venus Express**: ESA mission to Venus (2005-2015, atmospheric entry January 2015)

## License

The C reference implementation is provided by ESA. See `c-reference/README.md` for provenance details.

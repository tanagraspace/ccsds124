# POCKET+ Rust Implementation

[![Build](https://github.com/tanagraspace/pocket-plus/actions/workflows/rust-build.yml/badge.svg)](https://github.com/tanagraspace/pocket-plus/actions/workflows/rust-build.yml)
[![Coverage](assets/coverage.svg)](https://tanagraspace.com/pocket-plus/rust/coverage/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A Rust implementation of the [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf) POCKET+ lossless compression algorithm of fixed-length housekeeping data.

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
cargo build --release    # Build library and CLI
cargo test               # Run all tests
cargo clippy             # Run linter
cargo fmt                # Format code
cargo doc --open         # Generate API documentation
make bench               # Run benchmarks
make clean               # Clean build artifacts
```

### Docker

```bash
docker-compose run --rm rust                    # Build, test
docker-compose run --rm --build rust            # Rebuild after changes
```

## CLI

```bash
# Compress
./target/release/pocketplus <input> <packet_size> <pt> <ft> <rt> <robustness>

# Decompress
./target/release/pocketplus -d <input.pkt> <packet_size> <robustness>
```

**Example:**
```bash
./target/release/pocketplus data.bin 90 10 20 50 1      # -> data.bin.pkt
./target/release/pocketplus -d data.bin.pkt 90 1        # -> data.bin.depkt
```

Run `./target/release/pocketplus --help` for full usage.

## Library Usage

```rust
use pocketplus::{compress, decompress};

// Compress
let compressed = compress(
    &input_data,
    720,    // packet_size in bits
    1,      // robustness (0-7)
    10,     // pt_limit
    20,     // ft_limit
    50,     // rt_limit
).unwrap();

// Decompress
let decompressed = decompress(&compressed, 720, 1).unwrap();
```

**Note:** the high-level `compress()`/`decompress()` byte-buffer APIs require `packet_size` to be a multiple of 8 bits. The CCSDS 124.0-B-1 standard allows any F in 1..=65535 bits — use the low-level `Compressor`/`Decompressor` for non-byte-aligned packet lengths.

## Design

- **Zero dependencies** - Rust standard library only
- **Byte-identical output** - Matches C reference implementation exactly
- **Safe Rust** - No unsafe code (`#![forbid(unsafe_code)]`)
- **Pedantic linting** - `clippy::pedantic` enabled

## File Structure

```
implementations/rust/
├── src/
│   ├── lib.rs           # Public API
│   ├── bitvector.rs     # Fixed-length bit vectors
│   ├── bitbuffer.rs     # Variable-length output buffer
│   ├── bitreader.rs     # Sequential bit reading
│   ├── encode.rs        # COUNT, RLE, BE encoding
│   ├── decode.rs        # COUNT, RLE decoding
│   ├── mask.rs          # Mask update logic
│   ├── compress.rs      # Compression algorithm
│   ├── decompress.rs    # Decompression algorithm
│   ├── error.rs         # Error types
│   └── bin/
│       ├── pocketplus.rs # Command-line interface
│       └── bench.rs      # Performance benchmarks
└── tests/
    ├── vectors.rs       # Reference vector validation
    └── test_cli.sh      # CLI round-trip tests
```

## API

### High-Level

- `compress()` / `decompress()` - Compress/decompress entire buffer

### Low-Level

- `Compressor::compress_packet()` / `Decompressor::decompress_packet()` - Single packet
- `count_encode()` / `count_decode()` - Counter encoding (Eq. 9)
- `rle_encode()` / `rle_decode()` - Run-length encoding (Eq. 10)
- `bit_extract()` / `bit_insert()` - Bit extraction (Eq. 11)

## References

- [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf)
- [ESA POCKET+](https://opssat.esa.int/pocket-plus/)

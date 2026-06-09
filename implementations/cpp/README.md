# POCKET+ C++ Implementation

[![C++ Build](https://github.com/tanagraspace/pocket-plus/actions/workflows/cpp-build.yml/badge.svg)](https://github.com/tanagraspace/pocket-plus/actions/workflows/cpp-build.yml) [![Lines](https://raw.githubusercontent.com/tanagraspace/pocket-plus/main/implementations/cpp/assets/coverage-lines.svg)](https://tanagraspace.com/pocket-plus/cpp/coverage/) [![Functions](https://raw.githubusercontent.com/tanagraspace/pocket-plus/main/implementations/cpp/assets/coverage-functions.svg)](https://tanagraspace.com/pocket-plus/cpp/coverage/) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Header-only C++17 implementation of the [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf) POCKET+ lossless compression algorithm of fixed-length housekeeping data.

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

- C++17 compiler (GCC 7+, Clang 5+, MSVC 2019+)
- CMake 3.14+

## Building

```bash
make               # Build library and CLI
make test          # Run all tests
make coverage      # Run tests with coverage (text summary)
make coverage-html # Generate HTML coverage report
make docs          # Generate API documentation (requires doxygen)
make clean         # Clean build artifacts
```

### Docker

```bash
docker-compose run --rm cpp                    # Build, test
docker-compose run --rm --build cpp            # Rebuild after changes
```

## CLI

```bash
# Compress
./build/pocketplus input.bin 90 20 50 100 2
# Output: input.bin.pkt

# Decompress
./build/pocketplus -d input.bin.pkt 90 2
# Output: input.bin.depkt

# Help
./build/pocketplus --help

# Version
./build/pocketplus --version
```

## Library Usage

This is a **header-only library**. Simply include the headers in your project - no linking required (except for the CLI tool). All template implementations are in the `include/pocketplus/` headers.

### High-Level API

```cpp
#include <pocketplus/pocketplus.hpp>

// Compress 90-byte packets (720 bits) with robustness=2
std::uint8_t input[9000];    // 100 packets
std::uint8_t output[18000];
std::size_t output_size;

auto result = pocketplus::compress<720>(
    input, sizeof(input),
    output, sizeof(output),
    output_size,
    2,      // robustness
    20,     // pt_limit
    50,     // ft_limit
    100     // rt_limit
);

// Decompress
std::uint8_t decompressed[9000];
std::size_t decompressed_size;

result = pocketplus::decompress<720>(
    output, output_size,
    decompressed, sizeof(decompressed),
    decompressed_size,
    2       // robustness
);
```

### Low-Level API

```cpp
#include <pocketplus/compressor.hpp>
#include <pocketplus/decompressor.hpp>

using namespace pocketplus;

// Create compressor for 720-bit packets with robustness=2
Compressor<720> comp(2);
Decompressor<720> decomp(2);

// Compress a single packet
BitVector<720> input_packet;
input_packet.from_bytes(data, 90);

CompressParams params;
params.uncompressed_flag = true;  // First packet

BitBuffer<4320> output;
comp.compress_packet(input_packet, output, &params);

// Decompress
std::uint8_t output_bytes[540];
output.to_bytes(output_bytes, sizeof(output_bytes));
BitReader reader(output_bytes, output.size());

BitVector<720> output_packet;
decomp.decompress_packet(reader, output_packet);
```

## Design

- **Zero runtime dependencies** - Standard library only
- **Header-only templates** - Easy integration, compile-time size optimization
- **Static allocation** - No dynamic memory allocation (embedded-friendly)
- **32-bit optimized** - Uses `uint32_t` word storage for bit operations
- **Embedded compatible** - Works with `-fno-exceptions -fno-rtti`
- **Byte-identical output** - Matches C reference implementation exactly

## File Structure

```
include/pocketplus/
├── config.hpp       # Configuration constants
├── error.hpp        # Error codes
├── bitvector.hpp    # Fixed-length bit vectors
├── bitbuffer.hpp    # Variable-length output buffer
├── bitreader.hpp    # Sequential bit reading
├── encoder.hpp      # COUNT, RLE, BE encoding
├── decoder.hpp      # COUNT, RLE decoding
├── mask.hpp         # Mask update operations
├── compressor.hpp   # Compression algorithm
├── decompressor.hpp # Decompression algorithm
└── pocketplus.hpp   # High-level API
```

## Safety-Critical Code Compliance

This implementation follows safety-critical coding practices aligned with industry standards commonly used in space and embedded systems.

### Enabled Checks

The CI pipeline runs static analysis using:

| Tool | Checks | Purpose |
|------|--------|---------|
| **clang-tidy** | CERT C++, HIC++, C++ Core Guidelines, bugprone, performance | Safety/security analysis |
| **cppcheck** | All checks (errors, warnings, performance) | Generic static analysis |

These checks overlap significantly with **MISRA C++** guidelines, covering:
- Memory safety and bounds checking
- Undefined behavior prevention
- Type safety and implicit conversions
- Resource management
- Exception safety

### MISRA C++ Note

Full MISRA C++:2023/2008 compliance checking requires commercial tools such as:
- [Cppcheck Premium](https://www.cppcheck.com/) - Full MISRA C++ 2023 support
- Polyspace, PC-lint, LDRA - Enterprise static analysis

The open-source checks enabled here provide comparable coverage for the most critical safety rules.

### Configuration Files

| File | Purpose |
|------|---------|
| `.clang-tidy` | clang-tidy check configuration |
| `safety.supp` | Documented deviations with rationale |

### Running Locally

```bash
# Generate compile_commands.json
cd build && cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run clang-tidy
clang-tidy -p build ../include/pocketplus/*.hpp

# Run cppcheck
cppcheck --enable=all --std=c++17 -Iinclude include/pocketplus/*.hpp
```

## References

- [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf)
- [POCKET+ on OPS-SAT-1 (SmallSat 2022)](https://digitalcommons.usu.edu/smallsat/2022/all2022/133/)

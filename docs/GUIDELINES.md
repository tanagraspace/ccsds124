# CCSDS 124.0-B-1 Implementation Guidelines

## Before You Start

**Read [GOTCHAS.md](GOTCHAS.md) first!** It documents 21 critical pitfalls that will cause your implementation to fail silently.

## Validation Requirements

Your implementation must produce **byte-for-byte identical output** to the reference for all test vectors:

| Test Vector | Input | Expected Output | Parameters |
|-------------|-------|-----------------|------------|
| simple | 9,000 bytes | 641 bytes | R=1, pt=10, ft=20, rt=50 |
| hiro | 9,000 bytes | 1,533 bytes | R=7, pt=10, ft=20, rt=50 |
| housekeeping | 900,000 bytes | 223,078 bytes | R=2, pt=20, ft=50, rt=100 |
| edge-cases | 45,000 bytes | 10,124 bytes | R=1, pt=10, ft=20, rt=50 |
| venus-express | 13,608,000 bytes | 5,891,500 bytes | R=2, pt=20, ft=50, rt=100 |

Test vectors location: `test-vectors/`

## Critical Implementation Details

These are **not obvious** from the CCSDS spec:

1. **Vt calculation** - Start from position Rt+1, not position 2
2. **ct calculation** - Include current packet's pt flag (Vt+1 total entries)
3. **kt extraction** - Use forward order (low to high position)
4. **BE extraction** - Use reverse order (high to low position)
5. **Flag counters** - Use countdown counters, not modulo arithmetic
6. **Init phase** - First Rt+1 packets have ft=1, rt=1, pt=0
7. **D₀ = 0** - Set prev_mask = mask at init so D₀ = M₀ XOR M₀ = 0
8. **NOT masking** - MSB-aligned: mask high bits, not low bits, for non-byte-aligned vectors
9. **Decoder validation** - Validate bitstream integrity (sufficient bits, valid RLE deltas, consumed padding) to reject corrupt packets per CCSDS cross-validation expectations

## Implementation Checklist

### Core
- Compression produces identical output to reference
- Robustness levels R=1, R=2, and R=7 work
- All 5 test vectors pass

### Components
- COUNT encoding (variable-length integer)
- RLE encoding (run-length with terminator)
- BE (bit extraction) - reverse order
- kt extraction - forward order with mask inversion

### State Management
- Change history buffer (16 entries for Vt)
- Flag history buffer (16 entries for ct)
- Countdown counters (pt, ft, rt)

## Testing Strategy

1. **Start with simple.bin** (R=1) - validates basic algorithm
2. **Then hiro.bin** (R=7) - validates high robustness levels
3. **Then housekeeping.bin** (R=2) - validates Vt calculation
4. **Then edge-cases.bin** (R=1) - validates ct calculation
5. **Finally venus-express** (R=2, large) - validates at scale
6. **CCSDS cross-validation** (24,900 vectors) - validates non-byte-aligned F, non-zero M₀, packet loss

If simple passes but others fail, check Vt and ct calculations.
If cross-validation fails but reference vectors pass, check D₀ initialization and NOT masking for non-byte-aligned lengths.

## Language-Specific Notes

### C
- Use fixed-size types (`uint8_t`, `uint32_t`)
- Minimize dynamic allocation
- See `implementations/c/` for reference

### C++

**Compatibility:**
- C++17 minimum (GCC 7+, Clang 5+, MSVC 2019+)
- Zero runtime dependencies (standard library only)
- Header-only templates for easy integration
- Works with `-fno-exceptions -fno-rtti` (embedded systems)

**Code Style:**
- clang-format for formatting
- clang-tidy for static analysis
- Use `Error` enum for error handling (no exceptions)

**Testing:**
```bash
cd implementations/cpp
make build            # Build library, CLI, and tests
make test             # Run tests
make coverage         # Run tests with coverage
```

**Structure:**
- Core library: `include/ccsds124/` (header-only)
- CLI: `src/cli.cpp`
- Uses 32-bit word storage for bit vectors (same as C)

### Python

**Compatibility:**
- Zero external runtime dependencies (standard library only)
- MicroPython compatible (ESP32, RP2040, etc.)
- Python 3.9+ and MicroPython 1.17+

**Code Style:**
- Ruff for formatting and linting (`ruff format`, `ruff check`)
- mypy for static type checking (CI only)
- Simple type hints without `typing` module imports:
  ```python
  # Good - MicroPython compatible
  def compress(data: bytes, packet_size: int) -> bytes:
      ...

  # Avoid - requires typing module
  from typing import Optional, List
  def compress(data: bytes) -> Optional[bytes]:
      ...
  ```

**CLI:**
- Use simple `sys.argv` parsing (no `argparse` - not in MicroPython)

**Structure:**
- Core library: `ccsds124/` package - compression/decompression modules
- CLI: `cli.py` - command-line interface
- Avoid large intermediate allocations (memory-constrained devices)

### Go

**Compatibility:**
- Zero external dependencies (standard library only)
- Go 1.21+ supported
- Idiomatic Go patterns (error returns, interfaces)

**Code Style:**
- `gofmt` for formatting (enforced by CI)
- `go vet` for static analysis
- Return errors, don't panic

**Testing:**
```bash
cd implementations/go
go test -v ./...           # Run tests
go test -race ./...        # Race detection
go test -cover ./...       # Coverage
```

**Structure:**
- Core library: `ccsds124/` package
- CLI: `cmd/ccsds124/main.go`
- Uses 32-bit word storage for bit vectors (optimized for performance)

### Rust

**Compatibility:**
- Zero external runtime dependencies (standard library only)
- Rust 2021 edition (1.56+)
- Memory-safe with zero-copy where possible

**Code Style:**
- `cargo fmt` for formatting (enforced by CI)
- `cargo clippy` for linting
- Use `Result<T, E>` for error handling, avoid `unwrap()` in library code

**Testing:**
```bash
cd implementations/rust
cargo test                # Run tests
cargo bench               # Run benchmarks
cargo clippy              # Lint
```

**Structure:**
- Core library: `src/lib.rs`
- CLI: `src/bin/ccsds124.rs`
- Uses 32-bit word storage for bit vectors

### Java

**Compatibility:**
- Zero external runtime dependencies (JDK 11+ only)
- Test-only dependencies (JUnit 5, JaCoCo)
- Enterprise/ground systems target

**Code Style:**
- Google Java Style Guide via Spotless
- Checkstyle for additional rules
- SpotBugs for static analysis

**Testing:**
```bash
cd implementations/java
mvn test                  # Run tests
mvn verify                # Full verification with coverage
mvn spotless:apply        # Format code
mvn checkstyle:check      # Check style
```

**Structure:**
- Core library: `src/main/java/space/tanagra/ccsds124/`
- CLI: `src/main/java/space/tanagra/ccsds124/cli/Main.java`
- Uses 32-bit word storage for bit vectors (same as C)

### Other Languages
- Port from C implementation
- Validate against same test vectors
- Ensure bit-level compatibility

## Documentation

- `ALGORITHM.md` - Detailed algorithm description
- `GOTCHAS.md` - Critical implementation pitfalls

## Quick Debugging

| Symptom | Likely Cause |
|---------|--------------|
| R=2 tests fail, R=1 passes | Vt starts at wrong position |
| edge-cases fails, simple passes | ct missing current flag |
| Bit-level errors mid-stream | kt using wrong extraction order |
| Size off by 10%+ | Flag timing wrong |
| First packet wrong with non-zero M₀ | D₀ = M₀ instead of 0 |
| Non-byte-aligned F fails | NOT masks low bits not high |

See [GOTCHAS.md](GOTCHAS.md) for detailed diagnosis.

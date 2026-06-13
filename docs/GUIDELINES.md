# CCSDS 124.0-B-1 Porting & Build Notes

Per-language build, test, and style notes for working on or porting the implementations. This is the *how to build it* companion to the other docs: for the byte-level pitfalls you must get right see the [Implementer's Guide](GOTCHAS.md), for the algorithm itself the [Algorithm Reference](ALGORITHM.md), and for conformance evidence [CONFORMANCE.md](CONFORMANCE.md).

## Before You Start

**Read the [Implementer's Guide](GOTCHAS.md) first.** It documents the 21 byte-level pitfalls that will otherwise make your implementation fail silently.

## Validation Requirements

Your implementation must produce **byte-for-byte identical output** to the ESA reference for all five reference vectors (in `test-vectors/`). The vectors, their parameters, and expected output sizes are listed in [TESTING.md → Reference Test Vectors](TESTING.md#reference-test-vectors); the overall conformance bar is in [CONFORMANCE.md](CONFORMANCE.md).

## Critical Implementation Details

The non-obvious, spec-divergent pitfalls — the Vₜ window, cₜ, kₜ extraction order, BE order, countdown counters, the init phase, D₀ = 0, MSB-aligned NOT, and decoder validation — are documented in full, with citations, examples, and fixes, in the [Implementer's Guide](GOTCHAS.md). Read it before implementing; this doc does not duplicate it.

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

- [Implementer's Guide](GOTCHAS.md) — byte-level pitfalls and how to get byte-identical output
- [Algorithm Reference](ALGORITHM.md) — encoding/decoding steps and equations
- [Conformance](CONFORMANCE.md) — conformance evidence and cross-validation results
- [Test Report](TESTING.md) — the engineering test suite

## Quick Debugging

See the [Implementer's Guide → Common Symptoms and Diagnosis](GOTCHAS.md#common-symptoms-and-diagnosis) for the symptom → cause → fix table.

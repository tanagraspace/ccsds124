# Instructions for AI Agents

This file provides guidance for AI agents (like Claude Code) working on the POCKET+ project.

## Project Overview

POCKET+ is a multi-language implementation of the CCSDS 124.0-B-1 lossless compression algorithm designed for spacecraft housekeeping data. This is a **monorepo** with independent implementations in C, C++, Python, Go, Rust, and Java.

## Repository Structure

```
pocket-plus/
├── implementations/
│   ├── c/          # C implementation (embedded/flight)
│   ├── cpp/        # C++ implementation (embedded/flight)
│   ├── python/     # Python implementation (MicroPython compatible)
│   ├── go/         # Go implementation
│   ├── rust/       # Rust implementation
│   └── java/       # Java implementation (enterprise/ground)
├── crossvalidation/                    # Shared cross-validation infrastructure
│   ├── file_list.csv                   # Canonical manifest (expected sizes + SHA-256)
│   ├── run_crossvalidation.sh          # Shared runner (parameterized for binary paths)
│   ├── c/                              # C harness (encoder + decoder + Dockerfile)
│   ├── sandbox/                        # ESA reference code harness
│   ├── cpp/                            # C++ harness (TODO: #93)
│   ├── go/                             # Go harness (TODO: #93)
│   ├── rust/                           # Rust harness (TODO: #93)
│   └── java/                           # Java harness (TODO: #93)
├── docs/           # Shared documentation
└── test-vectors/   # Shared test data
```

## Key Principles

1. **Independent Versioning**: Each language has its own version (e.g., `c/v1.0.0`, `python/v0.9.0`)
2. **Interoperability**: All implementations must be able to compress/decompress each other's output
3. **Shared Test Vectors**: Use common test data in `test-vectors/` for validation
4. **No Over-Engineering**: Keep implementations simple and focused on the task

## Versioning Rules

- Use **prefixed git tags**: `c/vX.Y.Z`, `cpp/vX.Y.Z`, `python/vX.Y.Z`, `go/vX.Y.Z`, `rust/vX.Y.Z`, `java/vX.Y.Z`
- Follow [Semantic Versioning](https://semver.org/)
- Update version in language-specific files:
  - C: `implementations/c/VERSION` and `implementations/c/include/pocketplus.h`
  - C++: `implementations/cpp/VERSION` and `implementations/cpp/include/pocketplus/config.hpp`
  - Python: `implementations/python/pyproject.toml` and `implementations/python/pocketplus/__init__.py`
  - Go: Git tag (Go uses tags directly)
  - Rust: `implementations/rust/Cargo.toml`
  - Java: `implementations/java/pom.xml` and `implementations/java/VERSION`

## Commit Conventions

Use [Conventional Commits](https://www.conventionalcommits.org/) with language prefix.

**IMPORTANT**: When working on a GitHub issue, include the issue number in the commit message title:

```
c: add compression function (#5)
python: fix decompression bug (#12)
go: update benchmarks (#8)
docs: clarify algorithm overview (#3)
chore: update build system (#1)
```

For commits without an associated issue:
```
c: add compression function
python: fix decompression bug
```

## When Working on Implementations

### C Implementation
- Target embedded systems and spacecraft processors
- Minimize dynamic memory allocation
- Use fixed-size integer types (`uint8_t`, etc.)
- Provide clear error codes
- Test with Makefile: `cd implementations/c && make test`
- Cross-validate: `docker-compose run --rm c-crossvalidation` (results saved to `implementations/c/build/crossvalidation-results/crossvalidation-results.txt`)

### C++ Implementation
- Target embedded systems with C++17 support
- Header-only templates for compile-time size optimization
- Zero dynamic allocation (embedded-friendly)
- Works with `-fno-exceptions -fno-rtti`
- Uses Catch2 for testing
- Test with Makefile: `cd implementations/cpp && make test`
- Format code: `clang-format -i`

### Python Implementation
- Include type hints for all public APIs
- Follow PEP 8 style guidelines
- Use pytest for testing
- Maintain compatibility with Python 3.9+ (per `pyproject.toml` `requires-python`)
- Install dev dependencies: `pip install -e ".[dev]"`

### Go Implementation
- Follow Go idioms and conventions
- Return errors, don't panic
- Include benchmarks for performance-critical code
- Run tests: `go test ./...`
- Format code: `go fmt ./...`

### Rust Implementation
- Follow Rust idioms and ownership patterns
- Use `Result<T, E>` for error handling
- Include benchmarks with Criterion
- Run tests: `cargo test`
- Format code: `cargo fmt`
- Lint: `cargo clippy`

### Java Implementation
- Target enterprise/ground systems (JDK 11+)
- Zero runtime dependencies (test-only dependencies allowed)
- Use Maven for builds
- Follow Google Java Style Guide
- Run tests: `mvn test`
- Format code: `mvn spotless:apply`
- Lint: `mvn checkstyle:check` and `mvn spotbugs:check`

## Testing Requirements

1. **Unit Tests**: Test individual functions
2. **Round-trip Tests**: Compress then decompress, verify original data
3. **Test Vectors**: Validate against shared test vectors in `test-vectors/`
4. **Cross-Implementation**: Verify interoperability between languages

## Documentation

- Keep language-specific docs in each implementation's README
- Update shared docs in `docs/` when changing algorithm details
- Reference the CCSDS 124.0-B-1 specification when relevant
- Provide code examples in implementation READMEs

## Important Files to Check

- `docs/GOTCHAS.md` - Critical implementation pitfalls (read first!)
- `docs/GUIDELINES.md` - Implementation quick start
- `docs/ALGORITHM.md` - Algorithm specification
- `test-vectors/README.md` - Test data format and usage
- Each implementation's `README.md` and `CHANGELOG.md`

## Cross-Validation

Cross-validation runs encoder and decoder harnesses against 24,900 test vectors (7,935 encoder + 16,965 decoder) from the UAB suite and validates output via file size + SHA-256 against `crossvalidation/file_list.csv`.

### Running

```bash
# Docker (recommended, from project root)
docker-compose run --rm c-crossvalidation

# Local (from implementations/c/)
make crossvalidation
```

### Results

The runner script writes a results file (defaults to `build/crossvalidation-results.txt` when invoked via `make`). Override via `RESULTS_FILE` env var. The file contains a header (date, binaries, mode), per-phase pass/fail counts, failure details, and a final PASS/FAIL verdict.

The verdict honors the known-failures baseline (`crossvalidation/known-failures.txt`): failures that match the baseline exactly produce **PASS (matches known-failures baseline)**; only regressions or new failures produce **FAIL**. The baseline entries are documented gaps in the decoder accuracy guarantee logic (see `docs/TESTING.md` Known Gaps).

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `ENCODER_BIN` | Path to encoder harness binary | (required) |
| `DECODER_BIN` | Path to decoder harness binary | (required) |
| `RESULTS_FILE` | Path to results output file | `crossvalidation-results.txt` next to script |
| `KNOWN_FAILURES` | Path to known-failures baseline | `known-failures.txt` next to script |

### Adding a New Language Harness

1. Create `crossvalidation/<lang>/crossvalidation_encoder.<ext>` and `crossvalidation_decoder.<ext>`
2. Add a `crossvalidation/<lang>/Dockerfile` that builds the harness and sets `ENTRYPOINT` to invoke `run_crossvalidation.sh`
3. Add a `<lang>-crossvalidation` service to `docker-compose.yml`
4. The shared `run_crossvalidation.sh` is language-agnostic — just point `ENCODER_BIN` and `DECODER_BIN` at the compiled binaries

Harness ports for C++/Python/Go/Rust/Java are tracked in issue #93 (blocked on #89; decoder hardening prerequisite in #92) — don't start them without checking those issues first.

### Test Data

The cross-validation test vectors are **not committed**. Obtain them from ESA's OPS-SAT mission control team and extract them at the project root:

```bash
unzip ccsds124_full_crossvalidation_20220309.zip -d ccsds124_full_crossvalidation
```

## What NOT to Do

- ❌ Don't create unnecessary abstractions
- ❌ Don't add features beyond what's requested
- ❌ Don't modify the core algorithm without consulting CCSDS 124.0-B-1
- ❌ Don't break interoperability between implementations
- ❌ Don't add dependencies without good reason

## Reference Implementation

The `test-vector-generator/c-reference/` directory contains the ESA reference C implementation. Use it to understand the algorithm, but don't blindly copy it.

## Questions?

Refer to:
- `docs/GOTCHAS.md` and `docs/ALGORITHM.md`
- CCSDS 124.0-B-1 specification
- POCKET+ on OPS-SAT-1 (SmallSat 2022): https://digitalcommons.usu.edu/smallsat/2022/all2022/133/

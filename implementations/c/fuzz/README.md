# CCSDS 124.0-B-1 Fuzzing Infrastructure

This directory contains fuzzing harnesses for the CCSDS 124.0-B-1 C implementation using libFuzzer and AFL++.

## Harnesses

| File | Description |
|------|-------------|
| `fuzz_decompress.c` | Tests decompressor with arbitrary input |
| `fuzz_compress.c` | Tests compressor with arbitrary input |
| `fuzz_roundtrip.c` | Tests compress+decompress consistency |

## Requirements

### libFuzzer (recommended)

Requires Clang with libFuzzer support:

```bash
# macOS with Homebrew LLVM
brew install llvm
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"

# Or use Docker with ClusterFuzz image
docker run -it gcr.io/clusterfuzz-external/base
```

### AFL++

```bash
# Install AFL++
brew install afl++
# Or: apt-get install afl++
```

## Building

### libFuzzer Build

```bash
make
```

### AFL++ Build

```bash
make AFL=1
```

## Running

### Create Corpus

Generate initial seed files from test vectors:

```bash
make corpus
```

### Run Fuzzers

```bash
# Run decompressor fuzzer (1 hour)
make run-decompress

# Run compressor fuzzer (1 hour)
make run-compress

# Run roundtrip fuzzer (1 hour)
make run-roundtrip

# Quick test (1 minute each)
make quick
```

### Custom Runs

```bash
# Custom duration and parameters
./fuzz_decompress corpus_decompress/ -max_len=4096 -max_total_time=7200

# With specific seeds
./fuzz_roundtrip corpus_roundtrip/ -max_len=2048 -jobs=4
```

## Corpus Structure

Seed files use a header format:
- Byte 0: R parameter (masked to 0-7)
- Bytes 1-2: F parameter (big-endian, modulo'd)
- Byte 3: pt/ft parameters
- Remaining bytes: actual data

## Crashes

When a crash is found, libFuzzer creates files like:
- `crash-<hash>` - Input that caused the crash
- `leak-<hash>` - Input that caused memory leak
- `timeout-<hash>` - Input that caused timeout

Reproduce crashes:
```bash
./fuzz_decompress crash-abc123
```

## CI Integration

The fuzzing infrastructure is integrated with GitHub Actions via `.github/workflows/comprehensive-tests.yml` for automated short fuzz runs.

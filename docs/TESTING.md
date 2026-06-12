# CCSDS 124.0-B-1 Test Report

This document covers the comprehensive test suite for the C implementation, which serves as the reference implementation validated against ESA's original C code. Other implementations (C++, Python, Go, Rust, Java) validate correctness via shared test vectors but do not replicate the full test infrastructure (fuzzing, packet loss simulation, CCSDS-style generation).

## Prerequisites

All commands should be run from the repository root directory.

**Clean rebuild** (required after source changes):
```bash
docker-compose build c && docker-compose run c sh -c "make clean && make test"
```

## Unit Tests

**Goal**: Verify core components function correctly.

**Method**: Test individual modules with known inputs and expected outputs.

**Source**: [test_bitvector.c](../implementations/c/tests/test_bitvector.c), [test_bitbuffer.c](../implementations/c/tests/test_bitbuffer.c), [test_compress.c](../implementations/c/tests/test_compress.c), [test_decompress.c](../implementations/c/tests/test_decompress.c), [test_decompress_checked.c](../implementations/c/tests/test_decompress_checked.c), [test_discover.c](../implementations/c/tests/test_discover.c), [test_encode.c](../implementations/c/tests/test_encode.c), [test_mask.c](../implementations/c/tests/test_mask.c)

**Run**:
```bash
docker-compose run c make test
```

**Result**: Pass

| Module | Description |
|--------|-------------|
| Bit Vector | Init, get/set, XOR/OR/AND/NOT, shift, reverse, hamming weight, MSB-aligned NOT |
| Bit Buffer | Init, append bits, append bitvector, to_bytes, overflow |
| Compression | Compressor init, packet compression, CCSDS helpers, eₜ/kₜ/cₜ, D₀ zero initialization, manual mode |
| Decompression | Bit reader (including byte alignment), COUNT/RLE decode, packet decompression, error paths |
| Checked Decompression | Accuracy guarantee decision tree, mask sync, status ring (including 0x02 skip), state save/restore, rt=1 excess-bits tolerance |
| Packet Length Discovery | Reference packet discovery, various F values, non-discoverable packets, et=1/kt path, truncated reference, excess bits, edge cases |
| Encoding | COUNT encode, RLE encode, bit extraction |
| Mask | Update build/mask vectors, compute change vector |

## Malformed Input Tests

**Goal**: Verify proper rejection of invalid inputs.

**Method**: Feed invalid parameters (F=0, R>7, NULL pointers), truncated data, and boundary conditions. Verify appropriate error codes returned.

**Source**: [test_malformed.c](../implementations/c/tests/test_malformed.c)

**Run**:
```bash
docker-compose run c ./build/test_malformed
```

**Result**: Pass

| Category | Expected Behavior |
|----------|-------------------|
| Invalid Parameters (Compress) | F=0, F>max, R>7, NULL pointers rejected |
| Invalid Parameters (Decompress) | F=0, F>max, R>7 rejected |
| Truncated Data | Empty input, insufficient bytes, COUNT/RLE truncation handled |
| Corrupted Data | Corrupted streams detected or produce wrong output |
| Boundary Conditions | All-zeros, all-ones, alternating patterns, F=1, F=8192 |
| Output Buffer Overflow | Small buffer returns overflow error |
| API Input Validation | NULL arguments rejected at API level |
| Stress Tests | 100 identical packets, alternating packets |

## Robustness Parameter (R=0-7) Tests

**Goal**: Verify R parameter behavior per CCSDS 124.0-B-1 and bit-identical output to ESA reference.

**Method**:
- Roundtrip compression/decompression for each R value with varying packet counts
- Verify compression ratio increases with R (more robustness overhead)
- Compare compressed output byte-for-byte against ESA C reference for all R values

**Source**: [test_robustness.c](../implementations/c/tests/test_robustness.c), [test_reference.sh](../implementations/c/tests/test_reference.sh)

**Run**:
```bash
docker-compose run c ./build/test_robustness
docker-compose run c make test-reference
```

**Result**: Pass - All 8 R values produce identical output to reference.

| R | Output Size | Overhead vs R=0 | Max Consecutive Losses Tolerated |
|---|-------------|-----------------|----------------------------------|
| 0 | 706 bytes | baseline | 0 |
| 1 | 875 bytes | +24% | 1 |
| 2 | 1,036 bytes | +47% | 2 |
| 3 | 1,195 bytes | +69% | 3 |
| 4 | 1,345 bytes | +91% | 4 |
| 5 | 1,488 bytes | +111% | 5 |
| 6 | 1,625 bytes | +130% | 6 |
| 7 | 1,761 bytes | +149% | 7 |

**Test input**: 100 packets × 90 bytes = 9,000 bytes (housekeeping-like pattern)

## Packet Loss Recovery Tests

**Goal**: Validate recovery from simulated transmission losses.

**Method**:
- Compress packets with various R values
- Simulate packet loss by dropping packets
- Notify decompressor via `ccsds124_decompressor_notify_packet_loss()`
- Verify: loss ≤ R allows recovery, loss > R causes corruption

**Source**: [test_packet_loss.c](../implementations/c/tests/test_packet_loss.c)

**Run**:
```bash
docker-compose run c ./build/test_packet_loss
```

**Result**: Pass

| Test | R Value | Loss Count | Expected | Actual |
|------|---------|------------|----------|--------|
| No loss baseline | 0-7 | 0 | Roundtrip OK | ✓ |
| R=0 with 1 loss | 0 | 1 | Corrupted | ✓ |
| R=1 with 1 loss | 1 | 1 | Recovered | ✓ |
| R=1 with 2 losses | 1 | 2 | Corrupted | ✓ |
| R=2 with 2 losses | 2 | 2 | Recovered | ✓ |
| R=3 with 3 losses | 3 | 3 | Recovered | ✓ |
| Init phase (first R+1 packets) | 0-7 | varies | rt=1 sync | ✓ |
| Periodic rt=1 sync | 1-7 | 1 | Recovered | ✓ |

**Key insight**: The effective robustness Vₜ = Rₜ + Cₜ can exceed R when consecutive packets have no changes (Cₜ > 0), allowing recovery from more losses than R alone would suggest.

## Reference Test Vectors

**Goal**: Byte-for-byte validation against ESA C reference implementation.

**Method**: Compress/decompress pre-generated test vectors (simple, housekeeping, edge-cases, hiro, venus-express). Compare output hashes.

**Source**: [test_vectors.c](../implementations/c/tests/test_vectors.c)

**Run**:
```bash
docker-compose run c ./build/test_vectors
```

**Result**: Pass

| Vector | Input Size | Compressed | Ratio | Description |
|--------|------------|------------|-------|-------------|
| simple | 9 KB | 641 bytes | 14.04x | High compressibility patterns |
| housekeeping | 900 KB | 223 KB | 4.03x | Realistic telemetry simulation |
| edge-cases | 45 KB | 10.1 KB | 4.44x | Mixed stable/changing sections |
| hiro | 9 KB | 1,533 bytes | 5.87x | High robustness (R=7) |
| venus-express | 13.6 MB | 5.9 MB | 2.31x | Real Venus Express ADCS telemetry |

## Fuzzing

**Goal**: Find crashes and edge cases via random input testing.

**Method**: LibFuzzer with three harnesses (compress, decompress, roundtrip). 60 seconds per fuzzer.

**Source**: [fuzz/](../implementations/c/fuzz/)

**Run**:
```bash
docker-compose run c-fuzz                         # 60 seconds per fuzzer
FUZZ_DURATION=3600 docker-compose run c-fuzz      # Extended (1 hour each)
```

**Result**: Pass - Zero crashes across ~922,000 random inputs.

| Fuzzer | Iterations | Crashes |
|--------|------------|---------|
| Decompress | 309,996 | 0 |
| Compress | 301,920 | 0 |
| Roundtrip | ~310,000 | 0 |

**Note**: LeakSanitizer may report small leaks (56 bytes) at fuzzer shutdown. These are false positives from libFuzzer's internal bookkeeping on Alpine/musl, not from CCSDS 124.0-B-1 code. The library uses static allocation with no malloc/free.

## CCSDS-Style Test Vector Generation

**Goal**: Replicate CCSDS standardization validation approach with GB-scale test vectors containing fault injection.

**Method**: Generate test vectors with faults injected into the compressed stream:
- **LOST packets**: Removed from stream to test R parameter recovery
- **MALFORMED packets**: Corrupted data (truncated, bit flips, zeros, random) to test error handling

**Source**: [generate.py](../test-vector-generator/input-generators/generate.py), [validate_ccsds_style.py](../test-vector-generator/scripts/validate_ccsds_style.py)

**Run** (from `test-vector-generator/input-generators/`):
```bash
# Full CCSDS-style (both fault types)
python generate.py -o ./vectors -s 1GB \
    --inject-lost --lost-frequency 50 \
    --inject-malformed --malformed-frequency 100 \
    --seed 42

# Validate generated vectors
python ../scripts/validate_ccsds_style.py --vectors ./vectors
```

### 50 MB Test Results

| R Value | Input | Compressed | Ratio | Normal | Lost | Malformed | Recovered |
|---------|-------|------------|-------|--------|------|-----------|-----------|
| R=0 | 6.6 MB | 1.75 MB | 3.74x | 71,361 | 728 | 728 | 0 (expected) |
| R=1 | 6.6 MB | 2.14 MB | 3.07x | 71,361 | 728 | 728 | 728 |
| R=2 | 6.6 MB | 2.49 MB | 2.63x | 71,361 | 728 | 728 | 728 |
| R=3 | 6.6 MB | 2.81 MB | 2.33x | 71,361 | 728 | 728 | 728 |
| R=4 | 6.6 MB | 3.15 MB | 2.08x | 71,361 | 728 | 728 | 728 |
| R=5 | 6.6 MB | 3.47 MB | 1.89x | 71,361 | 728 | 728 | 728 |
| R=6 | 6.6 MB | 3.80 MB | 1.73x | 71,361 | 728 | 728 | 728 |
| R=7 | 6.6 MB | 4.09 MB | 1.60x | 71,361 | 728 | 728 | 728 |

**Summary**: 8/8 vectors passed. 570,888 normal packets, 5,824 lost markers, 5,824 malformed packets, 5,096 successful recoveries.

### 1 GB Test Results

| R Value | Input | Compressed | Ratio | Normal | Lost | Malformed | Recovered |
|---------|-------|------------|-------|--------|------|-----------|-----------|
| R=0 | 134 MB | 36.0 MB | 3.73x | 1,461,482 | 14,913 | 14,913 | 0 (expected) |
| R=1 | 134 MB | 43.9 MB | 3.06x | 1,461,482 | 14,913 | 14,913 | 14,913 |
| R=2 | 134 MB | 50.9 MB | 2.64x | 1,461,482 | 14,913 | 14,913 | 14,913 |
| R=3 | 134 MB | 57.8 MB | 2.32x | 1,461,482 | 14,913 | 14,913 | 14,913 |
| R=4 | 134 MB | 64.5 MB | 2.08x | 1,461,482 | 14,913 | 14,913 | 14,913 |
| R=5 | 134 MB | 71.1 MB | 1.89x | 1,461,482 | 14,913 | 14,913 | 14,913 |
| R=6 | 134 MB | 77.7 MB | 1.73x | 1,461,482 | 14,913 | 14,913 | 14,913 |
| R=7 | 134 MB | 84.2 MB | 1.59x | 1,461,482 | 14,913 | 14,913 | 14,913 |

**Summary**: 8/8 vectors passed. 11,691,856 normal packets, 119,304 lost markers, 119,304 malformed packets, 104,391 successful recoveries.

### Expected vs Actual Behavior

**Compression Ratio**: As expected, compression ratio decreases as R increases (3.73x at R=0 down to 1.59x at R=7) because higher robustness requires more overhead bits per packet.

**Packet Loss Recovery**:
- **R=0**: All 14,913 lost packets were unrecoverable (expected - R=0 provides no packet loss tolerance)
- **R≥1**: All lost packets (single packet losses at frequency 50) were recovered, as R≥1 tolerates at least 1 consecutive lost packet

**Malformed Packet Handling**: All 119,304 malformed packets across both tests were handled gracefully with zero crashes, demonstrating robust error handling.

**Result**: Pass - Implementation correctly implements CCSDS 124.0-B-1 robustness semantics.

## CCSDS Cross-Validation

**Goal**: Validate byte-for-byte correctness against the comprehensive CCSDS 124.0-B-1 cross-validation test suite produced by the Universitat Autonoma de Barcelona (UAB) team.

**Method**: Run encoder and decoder harnesses against all test vectors. Compare generated output file sizes and SHA-256 hashes against the canonical manifest (`file_list.csv`).

**Source**: [crossvalidation/](../crossvalidation/)

**Run**:
```bash
# Docker (recommended)
docker-compose run --rm c-crossvalidation

# Local
cd implementations/c
make crossvalidation \
  CROSSVAL_DIR=../../crossvalidation/c \
  CROSSVAL_SCRIPT=../../crossvalidation/run_crossvalidation.sh
```

**Prerequisites**: The cross-validation data (`ccsds124_full_crossvalidation/`) must be obtained separately and placed at the project root. See [crossvalidation/README.md](../crossvalidation/README.md) for details.

**Result**: Pass (encoder 100%, decoder 89.0% — matches known-failures baseline)

| Direction | Passed | Total | Rate | Description |
|-----------|--------|-------|------|-------------|
| Encoder | 7,935 | 7,935 | 100% | Compress `.raw+config` → `.124`, validate size + SHA-256 |
| Decoder | 15,102 | 16,965 | 89.0% | Decompress `.124+config` → `.raw+large_f`, validate size + SHA-256 |

**Total**: 23,037 of 24,900 cross-validation vectors passed

The encoder passes all 7,935 vectors. The decoder passes 15,102 of 16,965 vectors. The remaining 1,863 failures are documented gaps recorded in `crossvalidation/known-failures.txt` — the runner reports **PASS (matches known-failures baseline)** when actual failures match that list exactly, and **FAIL** only on regressions or new failures. The suite covers valid and invalid parameters, non-byte-aligned packet lengths, non-zero initial masks, packet loss scenarios, and edge cases. Four gotchas were discovered and fixed during cross-validation (see [GOTCHAS.md](GOTCHAS.md) #19, #20, #21, and #22).

### Reverse-Engineered Validation Rules

Three reference-decoder behaviors were reverse-engineered from the test vectors (improving the decoder from 14,924 to 15,102) and are implemented in `ccsds124_discover_packet_length()` and `ccsds124_decompress_packet_checked()`:

1. **Truncated reference packets still signal F**: when the bitstream runs out after `COUNT(F)` but before the full `I_t`, the signaled length "is to be considered" (cross-validation README). Reported as `CCSDS124_STATUS_TRUNCATED_LENGTH` for the output trailer.
2. **Signaled-length validity (v1.6)**: a signaled `COUNT(F)` is trusted only if in range (1–65535) and consistent with the packet's own RLE spans (X_t span ≤ F, full-mask span ≤ F).
3. **Reference packets tolerate excess trailing bits**: `rt=1` packets are self-delimiting via `COUNT(F)`; an oversized Received Packet Length means the remainder is ignored. Compressed (`rt=0`) packets keep the strict ≤7-padding-bits rule (v1.10).

### Known Gaps (1,863 decoder vectors)

All remaining failures are in the **accuracy guarantee accept/reject logic** — deciding whether a decompressed packet's output is guaranteed (`0x00`) or unguaranteed (`0x01`) in the presence of corruption. The CCSDS 124.0-B-1 standard does not specify these decision rules, and they cannot be fully reverse-engineered from the expected output files alone. The failures split into:

**~1,400 vectors — too strict** (we reject packets the reference accepts)

Pattern: fuzzed packets with corrupt `COUNT(F)` values. We reject via `count_f_mismatch`; the reference accepts some of them via an unidentified validation path. Removing the `count_f_mismatch` check regresses (to ~12,926 passes) because many corrupt packets then produce wrong output from shifted bit offsets.

**~460 vectors — too lenient** (we guarantee packets the reference rejects)

Pattern: `rt=1, ft=1, mask_inconsistent=1, mask_synced=0`. Our logic guarantees these via the `ft=1` branch of the reference-packet check. Making mask-inconsistency detection unconditional — or gating it on a mask-ever-initialized flag — regresses catastrophically (~3,600 vectors) because `ft=1` packets after heavy loss are legitimate resynchronization points. A clean-robustness-window gate was also tried with no effect.

**16 vectors — unknown excess-rejection rule**: the reference rejects certain `rt=1` packets with small excess bit counts (48–344) after F is established, while accepting large excesses (4K–64K) on the stream's first valid reference packet. The exact discriminator is unidentified; these 16 are accepted as a trade-off for the 118 vectors the excess-tolerance rule fixes.

**Path to 100%:** requires access to the UAB reference decoder source (or its accept/reject decision rules) — the categories interact, and rule combinations beyond the three implemented above produced net regressions. Tracked in [#89](https://github.com/tanagraspace/ccsds124/issues/89); see also GOTCHAS.md #22.

Other known gaps:
- Cross-validation harnesses for C++, Python, Go, Rust, and Java are not yet implemented (`crossvalidation/<lang>/` are placeholders) — tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93), deferred until #89 resolves. Those implementations are validated via the shared `test-vectors/` only.
- The C++/Python/Go/Rust/Java decoders do not yet validate bitstream integrity (GOTCHAS.md #21) — corrupt input can produce silent wrong output instead of an error. Tracked in [#92](https://github.com/tanagraspace/ccsds124/issues/92).

## Run All Tests

```bash
docker-compose run c
```

This executes the full test suite: build, unit tests, CLI tests, valgrind, MISRA checks, and coverage report.

## Other Implementations

Other implementations validate via shared test vectors only:

```bash
docker-compose run cpp      # C++
docker-compose run python   # Python
docker-compose run go       # Go
docker-compose run rust     # Rust
docker-compose run java     # Java
```

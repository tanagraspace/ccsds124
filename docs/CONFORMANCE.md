# CCSDS 124.0-B-1 Conformance

This document is the **standard-conformance evidence** for this project: which standard is implemented, how conformance is demonstrated, and where the known gaps are.

For the engineering test report (unit, malformed-input, robustness, packet-loss, fuzzing, reference-vector, and CCSDS-style generation tests), see [TESTING.md](TESTING.md). For the byte-level implementation pitfalls, see the [Implementer's Guide](GOTCHAS.md).

## Standard conformed to

**CCSDS 124.0-B-1**, *Robust Compression of Fixed-Length Housekeeping Data* — Blue Book, Issue 1, February 2023 ([PDF](https://ccsds.org/Pubs/124x0b1.pdf)). The algorithm is based on ESA's patented POCKET+.

## How conformance is demonstrated

Two independent, complementary checks:

1. **Byte-identical output to the ESA reference implementation.** The C implementation produces output identical, byte for byte, to ESA/ESOC's original C reference ([`test-vector-generator/c-reference/`](../test-vector-generator/c-reference/)) across all five shared reference vectors (simple, housekeeping, edge-cases, hiro, venus-express) and all robustness levels R=0–7. The other five implementations (C++, Python, Go, Rust, Java) are validated byte-for-byte against the same shared [test vectors](../test-vectors/). Per-vector detail: [TESTING.md → Reference Test Vectors](TESTING.md#reference-test-vectors) and [→ Robustness Parameter Tests](TESTING.md#robustness-parameter-r0-7-tests).
2. **The UAB/CNES cross-validation suite** — the standard's own conformance test bench, described below.

## CCSDS Cross-Validation

**Goal**: Validate byte-for-byte correctness against the comprehensive CCSDS 124.0-B-1 cross-validation test suite produced by the Universitat Autònoma de Barcelona (UAB) team under the technical supervision of CNES.

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

The encoder passes all 7,935 vectors. The decoder passes 15,102 of 16,965 vectors. The remaining 1,863 failures are documented gaps recorded in `crossvalidation/known-failures.txt` — the runner reports **PASS (matches known-failures baseline)** when actual failures match that list exactly, and **FAIL** only on regressions or new failures. The suite covers valid and invalid parameters, non-byte-aligned packet lengths, non-zero initial masks, packet loss scenarios, and edge cases. Four gotchas were discovered and fixed during cross-validation (see [GOTCHAS.md](GOTCHAS.md) #18, #19, #20, and #21).

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

**Path to 100%:** requires access to the UAB reference decoder source (or its accept/reject decision rules) — the categories interact, and rule combinations beyond the three implemented above produced net regressions. Tracked in [#89](https://github.com/tanagraspace/ccsds124/issues/89); see also GOTCHAS.md #21.

Other known gaps:
- Cross-validation harnesses for C++, Python, Go, Rust, and Java are not yet implemented (`crossvalidation/<lang>/` are placeholders) — tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93), deferred until #89 resolves. Those implementations are validated via the shared `test-vectors/` only.
- The C++/Python/Go/Rust/Java decoders do not yet validate bitstream integrity (GOTCHAS.md #20) — corrupt input can produce silent wrong output instead of an error. Tracked in [#92](https://github.com/tanagraspace/ccsds124/issues/92).

## Per-implementation conformance status

| Implementation | Byte-identical to ESA reference | UAB cross-validation |
|----------------|---------------------------------|----------------------|
| C | Yes — 5 reference vectors, R=0–7 | Yes — encoder 100%, decoder 89.0% (see above) |
| C++ | Yes — shared test vectors | Not yet — harness tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93) |
| Python | Yes — shared test vectors | Not yet — harness tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93) |
| Go | Yes — shared test vectors | Not yet — harness tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93) |
| Rust | Yes — shared test vectors | Not yet — harness tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93) |
| Java | Yes — shared test vectors | Not yet — harness tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93) |

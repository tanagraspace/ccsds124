# Cross-language differential test (issue #105)

A systemic regression guard against **"fixed-in-C-not-ported" drift**. Three latent
bugs this project hit — `D₀=0` (#98), MSB-aligned NOT masking (#103), decoder
bitstream hardening (#92) — were fixed in C but initially missed in the other
languages, because the only thing exercising the relevant inputs (non-byte-aligned
`F`, non-zero initial mask `M₀`) was C's UAB cross-validation, which the other
languages don't run.

This test makes every implementation compress the **same corpus** and asserts
byte-identical output to the **C reference** (which is UAB-cross-validation-proven
for these cases), plus a round-trip.

## Files

- **`gen_golden.c`** — generates the corpus and the golden output using the C
  reference. Hard-codes the cases (deterministic data) so the fixture is
  reproducible.
- **`cases.json`** — the committed fixture each language test reads. Per case:
  `F` (bits), `R`, period limits `pt`/`ft`/`rt`, `m0` (hex), `packets` (hex,
  one per packet, `ceil(F/8)` bytes each, padding bits zeroed), `flags` (the
  per-packet `{pt,ft,rt}` the C reference used — recorded so no language has to
  re-derive flag timing), and `compressed` (the golden hex).

## Corpus coverage

`F ∈ {9, 12, 100}` (non-byte-aligned) and `{720}` (byte-aligned sanity), with
zero and non-zero `M₀`, and a range of `R`/`pt`/`ft`/`rt`. Non-byte-aligned `F`
plus non-zero `M₀` are the inputs that expose the bug class.

## Driving recipe (uniform across all six languages)

```
init Compressor(F, M0, R, pt, ft, rt)
for each packet:
    compress_packet(BitVector(F) from packet bytes, explicit flags {pt,ft,rt})
    -> byte-pad the packet's output -> append
assert concatenated bytes == "compressed" golden
```

The high-level/bulk APIs in Go/Rust/Java are byte-only (cannot express
non-byte-aligned `F`), so the test drives the **low-level `Compressor`** in every
language. Flags are taken from the recorded `flags` rather than recomputed, so the
byte comparison isolates the codec (kₜ, cₜ, D₀, NOT-masking, …) from flag-timing
(which the per-language reference-vector tests already cover).

## Regenerating the golden

Only needed if the corpus or the C reference changes:

```bash
cd implementations/c && make            # build libccsds124.a
cd ../..
gcc -Iimplementations/c/include test-vectors/differential/gen_golden.c \
    -Limplementations/c/build -lccsds124 -o /tmp/gen_golden
/tmp/gen_golden > test-vectors/differential/cases.json
```

`gen_golden.c` self-verifies each case (per-packet reconstruct == bulk, and
round-trip) before emitting, so a non-zero exit means something is wrong.

## Per-language tests

Each implementation's own test suite reads `cases.json` and runs in its existing CI:

| Language | Test | Round-trip |
|----------|------|------------|
| Python | `implementations/python/tests/test_differential.py` | all cases |
| C++ | `implementations/cpp/tests/test_differential.cpp` | all cases |
| Go | `implementations/go/ccsds124/differential_test.go` | byte-aligned, zero-M₀ |
| Rust | `implementations/rust/src/differential_test.rs` | — |
| Java | `implementations/java/src/test/java/space/tanagra/ccsds124/DifferentialTest.java` | byte-aligned, zero-M₀ |

Every language asserts byte-identical compression for all cases (the encoder-bug
guard). The round-trip column reflects what each decoder API exposes: Python and
C++ drive a low-level decoder with `M₀`, so they round-trip every case; Go and
Java have a byte-only public decoder, so they round-trip the byte-aligned,
zero-`M₀` cases; Rust asserts compression only. Full round-trip is also covered
by C (`gen_golden.c` self-verifies it before emitting).

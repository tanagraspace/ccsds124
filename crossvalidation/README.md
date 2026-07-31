# CCSDS 124.0-B-1 Cross-Validation

Cross-validation against the comprehensive test vector suite produced by the Universitat Autonoma de Barcelona (UAB) team (Miguel Hernandez-Cabronero, Ian Blanes, and Joan Serra-Sagrista under the technical supervision of Mickael Bruno from CNES).

## Structure

```
crossvalidation/
  README.md                 # This file
  file_list.csv             # Canonical manifest (expected sizes + SHA-256 hashes)
  known-failures.txt        # Documented UAB/CNES compatibility-gap baseline (1,863 decoder vectors)
  run_crossvalidation.sh    # Shared runner script (parameterized for binary paths)
  interoperability/         # Pairwise interoperability runner (#48)
  c/                        # C implementation harness
  cpp/                      # C++ implementation harness (TODO: #93)
  go/                       # Go implementation harness (TODO: #93)
  rust/                     # Rust implementation harness (TODO: #93)
  java/                     # Java implementation harness (TODO: #93)
  sandbox/                  # Original ESA reference code harness
```

[`interoperability/`](interoperability/README.md) answers a different question
from `run_crossvalidation.sh`: instead of validating one implementation against
the UAB/CNES expected output, it compares two implementations against each other
on the same inputs, over separate processes.

Harnesses for the other languages are tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93) — deliberately deferred until the C UAB/CNES compatibility ruleset is finalized ([#89](https://github.com/tanagraspace/ccsds124/issues/89)), with decoder bitstream hardening ([#92](https://github.com/tanagraspace/ccsds124/issues/92)) as a prerequisite.

## Test Vectors

The suite contains **7,935 encoder** and **16,965 decoder** test vectors designed to stress-test implementations of the CCSDS 124.0-B-1 standard. Vectors include both valid and invalid parameters, packet loss scenarios, and edge cases.

## Methodology

1. Each harness binary reads an input test vector, processes it through the implementation's API, and writes the output to a file. The C decoder harness uses `ccsds124_discover_packet_length()` to discover F from the compressed bitstream and `ccsds124_decompress_packet_checked()` for decompression with accuracy guarantee tracking (mask synchronization, status history, state save/restore, and the guarantee decision tree).
2. The runner script computes the **file size** and **SHA-256 hash** of each generated output.
3. These are compared against the expected values in `file_list.csv` (the canonical manifest from the cross-validation suite).
4. A vector passes if both size and SHA-256 match.

## Binary Formats

### Encoder

- **Input** (`.raw+config`): 32-bit BE `large_f` + initial mask M_0 + per-packet [Flag Configuration Byte + packet content]
- **Output** (`.124`): Concatenation of byte-aligned compressed packets

Flag Configuration Byte: bit7=reserved, bit6=f(send mask), bit5=p(update mask), bit4=r(reference), bits3-0=R(robustness)

### Decoder

- **Input** (`.124+config`): Per-element [Reception Byte + length + compressed bitstream] or [Reception Byte with LSB=1 for lost packet]
- **Output** (`.raw+large_f`): Per-element [Status Byte + decoded content if status=0x00] + final 32-bit BE packet length

Status values: 0x00=success, 0x01=error/unguaranteed, 0x02=lost

See the cross-validation suite's own `README_crossvalidation.md` for the full binary format specification.

## Runner Script

The shared `run_crossvalidation.sh` script requires two environment variables:

- `ENCODER_BIN`: Path to the encoder harness binary
- `DECODER_BIN`: Path to the decoder harness binary

Optional:

- `RESULTS_FILE`: Path to the results output file (default: `crossvalidation-results.txt` next to the script)
- `KNOWN_FAILURES`: Path to the known-failures baseline (default: `known-failures.txt` next to the script)

```bash
ENCODER_BIN=./build/encoder DECODER_BIN=./build/decoder bash crossvalidation/run_crossvalidation.sh [encoder|decoder|both] [data_dir]
```

## Known-Failures Baseline

The C decoder has **1,863 documented UAB/CNES compatibility gaps** in the malformed-input accept/reject logic (see [docs/CONFORMANCE.md](../docs/CONFORMANCE.md#known-gaps-1863-decoder-vectors) for the full analysis). These vector names are recorded in `known-failures.txt`. The runner's verdict is:

- **PASS** — zero failures
- **PASS (matches known-failures baseline)** — every failure is listed in the baseline; any baseline entries that now pass are reported so the file can be trimmed
- **FAIL** — one or more failures are *not* in the baseline (a regression or new failure)

When a known failure is fixed, remove its line from `known-failures.txt`. Never add entries without investigating the cause.

## Prerequisites

The cross-validation test vector data is **not committed** to this repository. You must obtain it separately from ESA's OPS-SAT mission control team:

```bash
# From the project root:
unzip ccsds124_full_crossvalidation_20220309.zip -d ccsds124_full_crossvalidation
```

## Usage

### Docker (recommended)

From the project root:

```bash
# C implementation
docker-compose run --rm c-crossvalidation

# Original ESA reference (sandbox)
docker-compose run --rm c-crossvalidation-sandbox
```

### Local (C implementation)

```bash
cd implementations/c
make crossvalidation \
  CROSSVAL_DIR=../../crossvalidation/c \
  CROSSVAL_SCRIPT=../../crossvalidation/run_crossvalidation.sh
```

# Pairwise interoperability runner

Compares two CCSDS 124.0-B-1 implementations against each other on the same
inputs, running each one as a separate process.

This is the correctness and interoperability part of
[#48](https://github.com/tanagraspace/ccsds124/issues/48). Performance, memory
use and binary-size comparisons remain out of scope for this change.

## How it differs from `run_crossvalidation.sh`

| | `../run_crossvalidation.sh` | this runner |
|---|---|---|
| Question | does one implementation match the UAB/CNES expected output? | do two implementations agree with each other? |
| Inputs | the UAB/CNES suite (not distributed with the repository) | any files you supply |
| Reference | `file_list.csv` sizes and SHA-256 | the other implementation |
| Binaries | `ENCODER_BIN` and `DECODER_BIN` | any number of executables declared in a config file |

They answer different questions and neither replaces the other. `make test` and
`scripts/benchmark.sh` only exercise the implementations in this repository, so
neither says anything about an external encoder and our decoder.

The runner does not build or download external implementations, and contains no
third-party source code, headers, libraries or binaries. External executables are
supplied independently by the user and remain subject to their respective licence
terms.

## The five results

For each unique pair (X, Y); both cross-decoding directions are evaluated.

| | Property | What a failure means |
|---|---|---|
| **A** | X compress → X decompress → original | X is not self-consistent |
| **B** | Y compress → Y decompress → original | Y is not self-consistent |
| **C** | X compress → **Y** decompress → original | Y cannot read X's stream |
| **D** | Y compress → **X** decompress → original | X cannot read Y's stream |
| **E** | X's compressed stream is byte-identical to Y's | the encoders disagree on the bitstream |

**Bit-identity is not interoperability.** C and D can pass while E fails, when
both sides make different but legal encoding choices — a success, not a defect.
E can pass while B or C fails, which narrows the disagreement to decoding rather
than to the encoded format, without identifying the cause on its own.

A failing cell shows that a pair disagrees, not which side is at fault. A and B
help narrow it down: if X round-trips its own output and Y does not round-trip
its own, the evidence points at Y.

`E_bit_identical` has three states:

- `true` — both streams were produced and are identical
- `false` — both were produced and differ; `E_first_difference` locates the first
  differing byte and bit
- `null` — the comparison could not be made, because at least one stream was
  never produced; `E_detail` says which

## Executable contract

Every declared executable must accept:

```
<exe> compress   --in FILE --out FILE --json FILE --packet-bytes N --R n --pt n --ft n --rt n
<exe> decompress --in FILE --out FILE --json FILE --packet-bytes N --R n
```

It writes its payload to `--out`, a JSON object to `--json`, and exits 0 on
success. `impl`, `mode`, `rc`, `input_bytes` and `output_bytes` are mandatory;
`error` is optional; anything else is ignored. Full contract:
[`schemas/result.schema.json`](schemas/result.schema.json).

**Two distinct status codes.** The process exit code and the `rc` the
implementation declares in its JSON are recorded separately, as
`*_process_exit_code` and `*_implementation_rc`. An invocation counts as
successful only when both are 0, the JSON satisfies the contract, the output file
exists, and the declared `mode`, `input_bytes` and `output_bytes` match what is
on disk. A non-zero exit with `rc: 0` is reported as an inconsistency.

`impl` is the identifier the wrapper declares for itself. It is recorded as
`*_reported_impl` and never compared against the configured alias.

## Usage

Declare the executables. Start from
[`examples/implementations.example.json`](examples/implementations.example.json)
and keep your copy out of version control, since the paths are site-specific:

```json
{
  "implementations": [
    {"name": "tanagra-c", "executable": "/path/to/tanagra_harness"},
    {"name": "external-impl", "executable": "/path/to/external_harness", "timeout": 300}
  ]
}
```

List the cases. `input` is a raw file of concatenated fixed-length packets with no
header, and `pt`/`ft`/`rt` must match the parameters the vector was produced with,
since they change the bitstream:

```json
[
  {"name": "simple", "input": "../../test-vectors/input/simple.bin",
   "packet_bytes": 90, "R": 1, "pt": 10, "ft": 20, "rt": 50}
]
```

Case names and implementation aliases become directory names under `--workdir`
(`<case>/<encoder>/<decoder>/`), so they must match
`^[A-Za-z0-9][A-Za-z0-9_.-]*$`. Relative paths inside the two files resolve
against the files themselves, not against the current directory.

```bash
python3 crossvalidation/interoperability/run_correctness.py \
    --config   my-implementations.json \
    --cases    my-cases.json \
    --workdir  /tmp/ccsds124-interop \
    --out-json results/interop.json \
    --out-csv  results/interop.csv
```

Exit status: `0` all checks passed, `1` a reconstruction was incorrect or an
executable failed, `2` configuration or usage error. `E` alone being false is not
an error. Stale artefacts from a previous run are deleted before each invocation,
so a run that produces nothing cannot inherit an earlier result.

## Results

`--out-json` holds one object per case and pair, with the parameters, the SHA-256
of every artefact, both status codes per implementation, and the five results.
`--out-csv` writes the same rows flat, nested objects as JSON text.

## Requirements

Python 3.10 or newer, standard library only. The result contract is checked
directly in `run_correctness.py`, so no JSON-schema library is needed; a test
keeps the schema file and the in-code checks in step.

## Limits

- Whole-stream comparison. The first differing byte and bit are reported, but the
  bitstream is not decoded to explain a difference.
- One packet geometry per case; results do not generalise across `F`.
- Results depend on the version and build of the external implementation. Record
  its exact revision alongside any result.
- Reconstructions are compared exactly. The compressed stream may be byte-padded;
  the decompressed output may not differ from the input by a single byte.

## Tests

```bash
python3 -m unittest discover -s crossvalidation/interoperability/tests -v
```

The suite generates deterministic fake implementations at runtime, so it needs no
external binary and no network. It covers the five results and their three E
states, stale-artefact reuse, both status codes, JSON that disagrees with what is
on disk, configuration and case validation, unsafe names, timeouts, and paths
containing spaces.

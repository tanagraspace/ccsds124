# CCSDS 124.0-B-1

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

The definitive implementation of the [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf) lossless compression algorithm of fixed-length housekeeping data.

## Citation

If CCSDS 124.0-B-1 contributes to your research, please cite:

> D. Evans, G. Labrèche, D. Marszk, S. Bammens, M. Hernandez-Cabronero, V. Zelenevskiy, V. Shiradhonkar, M. Starcik, and M. Henkel. 2022. "Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1," *Proceedings of the Small Satellite Conference*, Communications, SSC22-XII-03. https://digitalcommons.usu.edu/smallsat/2022/all2022/133/

<details>
<summary>BibTeX</summary>

```bibtex
@inproceedings{evans2022ccsds124,
  author    = {Evans, David and Labrèche, Georges and Marszk, Dominik and Bammens, Samuel and Hernandez-Cabronero, Miguel and Zelenevskiy, Vladimir and Shiradhonkar, Vasundhara and Starcik, Mario and Henkel, Maximilian},
  title     = {Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1},
  booktitle = {Proceedings of the Small Satellite Conference},
  year      = {2022},
  note      = {SSC22-XII-03},
  url       = {https://digitalcommons.usu.edu/smallsat/2022/all2022/133/}
}
```

</details>

## About

POCKET+ is an European Space Agency (ESA) patented lossless compression algorithm implemented using very low-level instructions such as OR, XOR, AND, etc. It has been designed to run on spacecraft command and control processors with low CPU power available and tight real-time constraints. An earlier version of POCKET+ was flight-proven onboard both the Nanomind 3200 flight computer and SEPP payload computer of ESA's OPS-SAT-1 spacecraft.

The algorithm has been standardized by CCSDS as **CCSDS 124.0-B-1** for compressing fixed-length spacecraft housekeeping packets.

## Documentation

- 📘 **[Implementer's Guide](docs/GOTCHAS.md)** — if you are implementing CCSDS 124.0-B-1 yourself, start here. It documents the 21 byte-level pitfalls (each tagged as spec-mandated or a reference-conformance detail, with section/equation citations) that you must get right to produce byte-identical output.
- **[Algorithm Reference](docs/ALGORITHM.md)** — the encoding/decoding steps, equations, and worked examples.
- **[Conformance & Testing](docs/TESTING.md)** — test vectors, cross-validation results, and documented gaps.
- **[Implementation Guidelines](docs/GUIDELINES.md)** — per-language build, test, and style notes.

## Implementations

| Language | Version | Validated¹ | Cross-Validated² | Target | Location |
|----------|---------|------------|------------------|--------|----------|
| C | 1.0.0 | Yes | Yes | Bare-metal Embedded / Embedded Linux / Desktop / Server | [`implementations/c/`](implementations/c/) |
| C++ | 1.0.0 | Yes | No | Bare-metal Embedded / Embedded Linux / Desktop / Server | [`implementations/cpp/`](implementations/cpp/) |
| Python | 1.0.0 | Yes | No | Embedded Linux / Desktop / Server | [`implementations/python/`](implementations/python/) |
| Go | 1.0.0 | Yes | No | Embedded Linux / Desktop / Server | [`implementations/go/`](implementations/go/) |
| Rust | 1.0.0 | Yes | No | Embedded Linux / Desktop / Server | [`implementations/rust/`](implementations/rust/) |
| Java | 1.0.0 | Yes | No | Embedded Linux / Desktop / Server | [`implementations/java/`](implementations/java/) |

¹ Byte-for-byte validated against the ESA reference implementation via the shared [test vectors](test-vectors/).
² Validated against the UAB CCSDS 124.0-B-1 cross-validation suite (24,900 vectors): encoder 100%, decoder 89% with [documented gaps](docs/TESTING.md#known-gaps-1863-decoder-vectors) in the accuracy guarantee logic for fuzzed packets. Harnesses for the other implementations are tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93).

### Which implementation should I use?

**For bare-metal embedded systems**: Use the **C** or **C++** implementation. Both are suitable for resource-constrained systems. The C++ implementation is header-only with template-based size optimization and works with `-fno-exceptions -fno-rtti`. The C implementation is optimized for 32-bit microcontrollers (e.g., GomSpace Nanomind 3200 / AVR32 MCU).

**For payload computers**: All implementations run on embedded Linux systems such as payload processors (e.g., SEPP on OPS-SAT-1). Choose based on your runtime environment and preference — C/C++ for the smallest footprint, Python/Go/Rust/Java for ecosystem convenience.

**For ground systems and prototyping**: All implementations produce identical compression output. Use whichever language fits your toolchain.

## Repository Structure

```
ccsds124/
├── implementations/
│   ├── c/              # C implementation
│   ├── cpp/            # C++ implementation
│   ├── python/         # Python implementation
│   ├── go/             # Go implementation
│   ├── rust/           # Rust implementation
│   └── java/           # Java implementation
├── docs/               # Shared documentation
├── test-vectors/       # Shared test data for validation
├── LICENSE
└── README.md
```

## Quick Start

### Docker (recommended)

Build, test, and generate coverage for any implementation:

```bash
docker-compose run --rm c          # C implementation
docker-compose run --rm cpp        # C++ implementation
docker-compose run --rm python     # Python implementation
docker-compose run --rm go         # Go implementation
docker-compose run --rm rust       # Rust implementation
docker-compose run --rm java       # Java implementation
```

Artifacts are written to `implementations/<lang>/build/`.

### Local Build

#### C

```bash
cd implementations/c
make          # Build library and CLI
make test     # Run tests
make coverage # Run tests with coverage report
```

#### C++

```bash
cd implementations/cpp
make build    # Build library, CLI, and tests
make test     # Run unit tests
make coverage # Run tests with coverage report
```

#### Python

```bash
cd implementations/python
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -e ".[dev]"
pytest
```

#### Go

```bash
cd implementations/go
make build    # Build library
make test     # Run tests
make coverage # Run tests with coverage
make docs     # Generate documentation
```

#### Rust

```bash
cd implementations/rust
cargo build --release  # Build library and CLI
cargo test             # Run tests
cargo doc --open       # Generate and view documentation
```

#### Java

```bash
cd implementations/java
mvn package            # Build library and CLI
mvn test               # Run tests
mvn javadoc:javadoc    # Generate documentation
```

## Performance

**Key findings** (venus-express dataset, 151K packets, 13.6 MB):
- **C++** is the fastest implementation (~143 MB/s compression, ~146 MB/s decompression), also suitable for embedded systems
- **Java** offers strong performance (~86 MB/s compression) with JVM convenience
- **C** delivers reliable embedded-friendly performance (~59 MB/s compression, ~80 MB/s decompression)
- **Rust** provides good performance (~64 MB/s compression)
- **Go** trades some speed (~49 MB/s compression) for simplicity and fast compilation

All implementations produce identical compression output. Choose based on your deployment constraints and language ecosystem.

See [Benchmark Results](docs/BENCHMARK.md) for detailed comparison.

### Test Vectors

All vectors are synthetic test data except for venus-express, which contains real housekeeping telemetry from ESA's [Venus Express](https://www.esa.int/Science_Exploration/Space_Science/Venus_Express) mission.

| Vector | Packets | Size | R | pt | ft | rt |
|--------|---------|------|---|----|----|-----|
| simple | 100 | 9 KB | 1 | 10 | 20 | 50 |
| hiro | 100 | 9 KB | 7 | 10 | 20 | 50 |
| edge-cases | 500 | 45 KB | 1 | 10 | 20 | 50 |
| housekeeping | 10,000 | 900 KB | 2 | 20 | 50 | 100 |
| venus-express | 151,200 | 13.6 MB | 2 | 20 | 50 | 100 |

### Run Benchmarks

```bash
docker-compose run --rm benchmark
```

Results are written to `docs/BENCHMARK.md`.

**Note**: Python is excluded from benchmarks as it prioritizes readability over performance.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for versioning strategy, release process, and contribution guidelines.

## License

See [LICENSE](LICENSE) for details.

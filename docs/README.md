# CCSDS 124.0-B-1 Documentation

Shared documentation for all CCSDS 124.0-B-1 implementations.

## Contents

- [Implementer's Guide](GOTCHAS.md) - byte-level pitfalls; read this first if you are implementing CCSDS 124.0-B-1
- [Algorithm Reference](ALGORITHM.md) - algorithm specification and details
- [Conformance](CONFORMANCE.md) - conformance evidence and cross-validation results
- [Test Report](TESTING.md) - engineering test report and validation procedures
- [Porting & Build Notes](GUIDELINES.md) - per-language build, test, and style notes
- [Benchmark](BENCHMARK.md) - performance comparison across implementations
- [Cross-Validation](../crossvalidation/README.md) - CCSDS 124.0-B-1 cross-validation suite, runner, and known-failures baseline
- [Test Vectors](../test-vectors/README.md) - Validation data
- [Test Vector Generator](../test-vector-generator/README.md) - Deterministic test vector generation

## About CCSDS 124.0-B-1

CCSDS 124.0-B-1 is a lossless compression algorithm, based on ESA's patented POCKET+, using low-level bitwise operations (OR, XOR, AND). Designed for spacecraft processors with limited CPU and real-time constraints.

## Key Features

- **Lossless** - Perfect data reconstruction
- **Low complexity** - Bitwise operations only
- **Packet loss resilient** - Configurable robustness level
- **No latency** - One output per input packet

## References

- [POCKET+ on OPS-SAT-1 (SmallSat 2022)](https://digitalcommons.usu.edu/smallsat/2022/all2022/133/)
- [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf)

# POCKET+ Documentation

Shared documentation for all POCKET+ implementations.

## Contents

- [Algorithm](ALGORITHM.md) - Algorithm specification and details
- [Guidelines](GUIDELINES.md) - Implementation quick start
- [Gotchas](GOTCHAS.md) - Critical pitfalls (read this first!)
- [Benchmark](BENCHMARK.md) - Performance comparison across implementations
- [Testing](TESTING.md) - Test report and validation procedures
- [Cross-Validation](../crossvalidation/README.md) - CCSDS 124.0-B-1 cross-validation suite, runner, and known-failures baseline
- [Test Vectors](../test-vectors/README.md) - Validation data
- [Test Vector Generator](../test-vector-generator/README.md) - Deterministic test vector generation

## About POCKET+

POCKET+ (CCSDS 124.0-B-1) is an ESA-patented lossless compression algorithm using low-level bitwise operations (OR, XOR, AND). Designed for spacecraft processors with limited CPU and real-time constraints.

## Key Features

- **Lossless** - Perfect data reconstruction
- **Low complexity** - Bitwise operations only
- **Packet loss resilient** - Configurable robustness level
- **No latency** - One output per input packet

## References

- [POCKET+ on OPS-SAT-1 (SmallSat 2022)](https://digitalcommons.usu.edu/smallsat/2022/all2022/133/)
- [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf)

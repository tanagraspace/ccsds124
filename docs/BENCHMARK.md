# CCSDS 124.0-B-1 Benchmark Results

Performance comparison across CCSDS 124.0-B-1 implementations.

## Environment

| Property | Value |
|----------|-------|
| OS | Alpine Linux v3.20 |
| Architecture | aarch64 |
| CPU | ARMv8 |
| CPU Cores | 2 |
| Memory | 1.9 GB |
| Container | Docker (Alpine 3.20) |
| Date | 2025-12-15 05:07:22 UTC |

## Build Settings

| Language | Compiler/Runtime | Version | Optimization Flags |
|----------|------------------|---------|-------------------|
| C | GCC | 13.2.1 | `-O3 -flto` |
| C++ | G++ | 13.2.1 | `-O3` (CMake Release) |
| Go | Go | 1.22.10 | Default |
| Rust | rustc | 1.78.0 | `--release` (opt-level=3) |
| Java | OpenJDK | 21.0.8 | Default (JIT) |

## Test Vectors

All vectors are synthetic test data except for venus-express, which contains real housekeeping telemetry from ESA's [Venus Express](https://www.esa.int/Science_Exploration/Space_Science/Venus_Express) mission.

| Vector | Packets | Size | R | pt | ft | rt |
|--------|---------|------|---|----|----|-----|
| simple | 100 | 9.0 KB | 1 | 10 | 20 | 50 |
| hiro | 100 | 9.0 KB | 7 | 10 | 20 | 50 |
| housekeeping | 10000 | 900.0 KB | 2 | 20 | 50 | 100 |
| venus-express | 151200 | 13.6 MB | 2 | 20 | 50 | 100 |

## Compression Results

### Compression: simple (100 packets, 9.0 KB)

| Implementation | Time (ms) | Packets/sec | µs/pkt | Throughput (MB/s) |
|----------------|-----------|-------------|--------|-------------------|
| C              |      .087 |     1148501 |    .87 |             98.57 |
| C++            |      .029 |     3387533 |    .29 |            290.75 |
| Go             |      .105 |      943841 |   1.05 |             81.01 |
| Rust           |      .062 |     1602820 |    .62 |            137.57 |
| Java           |      .220 |      453473 |   2.20 |             38.92 |

### Compression: hiro (100 packets, 9.0 KB)

| Implementation | Time (ms) | Packets/sec | µs/pkt | Throughput (MB/s) |
|----------------|-----------|-------------|--------|-------------------|
| C              |      .098 |     1017915 |    .98 |             87.36 |
| C++            |      .036 |     2738975 |    .36 |            235.08 |
| Go             |      .240 |      415247 |   2.40 |             35.64 |
| Rust           |      .095 |     1045587 |    .95 |             89.74 |
| Java           |      .162 |      615233 |   1.62 |             52.80 |

### Compression: housekeeping (10000 packets, 900.0 KB)

| Implementation | Time (ms) | Packets/sec | µs/pkt | Throughput (MB/s) |
|----------------|-----------|-------------|--------|-------------------|
| C              |    11.530 |      867274 |   1.15 |             74.43 |
| C++            |     4.580 |     2183215 |    .45 |            187.38 |
| Go             |    14.742 |      678303 |   1.47 |             58.21 |
| Rust           |     9.770 |     1023524 |    .97 |             87.84 |
| Java           |     7.863 |     1271717 |    .78 |            109.15 |

### Compression: venus-express (151200 packets, 13608.0 KB)

| Implementation | Time (ms) | Packets/sec | µs/pkt | Throughput (MB/s) |
|----------------|-----------|-------------|--------|-------------------|
| C              |   218.478 |      692058 |   1.44 |             59.39 |
| C++            |    90.487 |     1670946 |    .59 |            143.41 |
| Go             |   266.963 |      566369 |   1.76 |             48.61 |
| Rust           |   201.812 |      749211 |   1.33 |             64.30 |
| Java           |   150.991 |     1001381 |    .99 |             85.94 |

## Decompression Results

### Decompression: simple (100 packets, 9.0 KB)

| Implementation | Time (ms) | Packets/sec | µs/pkt | Throughput (MB/s) |
|----------------|-----------|-------------|--------|-------------------|
| C              |      .026 |     3735524 |    .26 |            320.62 |
| C++            |      .022 |     4440497 |    .22 |            381.13 |
| Go             |      .104 |      954653 |   1.04 |             81.93 |
| Rust           |      .101 |      984155 |   1.01 |             84.47 |
| Java           |      .327 |      305754 |   3.27 |             26.24 |

### Decompression: hiro (100 packets, 9.0 KB)

| Implementation | Time (ms) | Packets/sec | µs/pkt | Throughput (MB/s) |
|----------------|-----------|-------------|--------|-------------------|
| C              |      .046 |     2143622 |    .46 |            183.98 |
| C++            |      .040 |     2445585 |    .40 |            209.90 |
| Go             |      .201 |      495933 |   2.01 |             42.56 |
| Rust           |      .139 |      716383 |   1.39 |             61.48 |
| Java           |      .649 |      153971 |   6.49 |             13.21 |

### Decompression: housekeeping (10000 packets, 900.0 KB)

| Implementation | Time (ms) | Packets/sec | µs/pkt | Throughput (MB/s) |
|----------------|-----------|-------------|--------|-------------------|
| C              |     5.651 |     1769551 |    .56 |            151.88 |
| C++            |     3.574 |     2797406 |    .35 |            240.10 |
| Go             |    17.200 |      581378 |   1.72 |             49.90 |
| Rust           |    13.717 |      728985 |   1.37 |             62.56 |
| Java           |    15.383 |      650046 |   1.53 |             55.79 |

### Decompression: venus-express (151200 packets, 13608.0 KB)

| Implementation | Time (ms) | Packets/sec | µs/pkt | Throughput (MB/s) |
|----------------|-----------|-------------|--------|-------------------|
| C              |   162.788 |      928809 |   1.07 |             79.72 |
| C++            |    88.901 |     1700752 |    .58 |            145.97 |
| Go             |   393.971 |      383784 |   2.60 |             32.94 |
| Rust           |   302.169 |      500381 |   1.99 |             42.94 |
| Java           |   320.415 |      471887 |   2.11 |             40.50 |

## Methodology

- **Iterations**: 100 (with 1 warmup iteration)
- **Packet size**: 90 bytes (720 bits)
- **Timing**: Wall-clock time for complete file processing

## Notes

- **Python** is excluded from benchmarks as it is not performance-focused.
  The Python implementation prioritizes readability and correctness over speed.
- Results may vary based on system load and hardware.
- Go benchmarks use the native `go test -bench` framework.
- All other implementations use custom benchmark binaries with consistent output format.

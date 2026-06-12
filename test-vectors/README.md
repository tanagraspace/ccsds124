# CCSDS 124.0-B-1 Test Vectors

Validation data for CCSDS 124.0-B-1 implementations.

## Test Vectors

| Name | Input | Output | Ratio | Robustness | Parameters |
|------|-------|--------|-------|------------|------------|
| simple | 9 KB (100 pkts) | 641 B | 14.04x | R=1 | pt=10, ft=20, rt=50 |
| hiro | 9 KB (100 pkts) | 1.5 KB | 5.87x | R=7 | pt=10, ft=20, rt=50 |
| housekeeping | 900 KB (10K pkts) | 223 KB | 4.03x | R=2 | pt=20, ft=50, rt=100 |
| edge-cases | 45 KB (500 pkts) | 10 KB | 4.44x | R=1 | pt=10, ft=20, rt=50 |
| venus-express | 13.6 MB (151K pkts) | 5.9 MB | 2.31x | R=2 | pt=20, ft=50, rt=100 |

All packets are 90 bytes.

## Directory Structure

```
test-vectors/
├── input/                  # Uncompressed input files
│   ├── simple.bin
│   ├── housekeeping.bin
│   ├── edge-cases.bin
│   ├── venus-express.ccsds
│   └── hiro.bin
└── expected-output/        # Reference compressed output
    ├── simple.bin.pkt
    ├── housekeeping.bin.pkt
    ├── edge-cases.bin.pkt
    ├── venus-express.ccsds.pkt
    └── hiro.bin.pkt
```

## Usage

### C
```bash
cd implementations/c
make test  # Runs test_vectors against all test vectors
```

### Python
```bash
cd implementations/python
pytest tests/test_vectors.py -v
```

### Go
```bash
cd implementations/go
go test -v ./ccsds124/ -run TestVector
```

## Validation

A correct implementation must produce **byte-for-byte identical** output to the expected-output files. Any difference indicates an implementation bug.

## Generation

Test vectors are generated using the ESA reference implementation. See `test-vector-generator/` for the Docker-based generation pipeline.

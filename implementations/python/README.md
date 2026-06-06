# POCKET+ Python Implementation

[![Python Build](https://github.com/tanagraspace/pocket-plus/actions/workflows/python-build.yml/badge.svg)](https://github.com/tanagraspace/pocket-plus/actions/workflows/python-build.yml)
[![MicroPython Build](https://github.com/tanagraspace/pocket-plus/actions/workflows/micropython-build.yml/badge.svg)](https://github.com/tanagraspace/pocket-plus/actions/workflows/micropython-build.yml)
[![Coverage](assets/coverage.svg)](https://tanagraspace.com/pocket-plus/python/coverage/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Python and MicroPython compatible implementation of the [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf) POCKET+ lossless compression algorithm of fixed-length housekeeping data.

## Citation

If POCKET+ contributes to your research, please cite:

> D. Evans, G. Labrèche, D. Marszk, S. Bammens, M. Hernandez-Cabronero, V. Zelenevskiy, V. Shiradhonkar, M. Starcik, and M. Henkel. 2022. "Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1," *Proceedings of the Small Satellite Conference*, Communications, SSC22-XII-03. https://digitalcommons.usu.edu/smallsat/2022/all2022/133/

<details>
<summary>BibTeX</summary>

```bibtex
@inproceedings{evans2022pocketplus,
  author    = {Evans, David and Labrèche, Georges and Marszk, Dominik and Bammens, Samuel and Hernandez-Cabronero, Miguel and Zelenevskiy, Vladimir and Shiradhonkar, Vasundhara and Starcik, Mario and Henkel, Maximilian},
  title     = {Implementing the New CCSDS Housekeeping Data Compression Standard 124.0-B-1 (based on POCKET+) on OPS-SAT-1},
  booktitle = {Proceedings of the Small Satellite Conference},
  year      = {2022},
  note      = {SSC22-XII-03},
  url       = {https://digitalcommons.usu.edu/smallsat/2022/all2022/133/}
}
```
</details>

## Installation

```bash
python -m venv venv
source venv/bin/activate   # On Windows: venv\Scripts\activate
pip install -e .           # Or: pip install -e ".[dev]" for development
```

### Docker

```bash
docker-compose run --rm python                # Lint, test, coverage, API docs
docker-compose run --rm --build python        # Rebuild after changes
```

Reports generated in `build/docs/`: `api/`, `coverage/`, `tests/`.

## Usage

### CLI

```bash
# Compress
python cli.py data.bin 90 10 20 50 1

# Decompress
python cli.py -d data.bin.pkt 90 1

# Help
python cli.py --help
```

### Python API

```python
from pocketplus import compress, decompress

# Compress: packet_size in bits, robustness 0-7
compressed = compress(data, packet_size=720, robustness=1,
                      pt_limit=10, ft_limit=20, rt_limit=50)

# Decompress
decompressed = decompress(compressed, packet_size=720, robustness=1)
```

## Testing

```bash
pytest                           # Run all tests
pytest --fast                    # Skip slow tests (venus-express, housekeeping)
pytest tests/test_vectors.py     # Reference validation only
```

Generate HTML reports:
```bash
pytest --cov=pocketplus --cov-report=html:build/docs/coverage \
       --html=build/docs/tests/report.html --self-contained-html
```

## Development

```bash
ruff format .                    # Format code
ruff check .                     # Lint
mypy pocketplus                  # Type check
pdoc -d google pocketplus -o build/docs/api  # Generate API docs
```

## Design

- **Zero dependencies** - Standard library only
- **MicroPython compatible** - No argparse, no typing imports
- **Byte-identical output** - Matches ESA reference implementation

## File Structure

```
implementations/python/
├── pocketplus/
│   ├── __init__.py          # Public API exports
│   ├── bitvector.py         # Fixed-length bit vectors
│   ├── bitbuffer.py         # Variable-length output buffer
│   ├── bitreader.py         # Sequential bit reading
│   ├── encode.py            # COUNT, RLE, BE encoding
│   ├── decode.py            # COUNT, RLE decoding
│   ├── mask.py              # Mask update logic
│   ├── compress.py          # Compression algorithm
│   └── decompress.py        # Decompression algorithm
├── cli.py                   # Command-line interface
└── tests/                   # Unit and integration tests
```

## API

### High-Level

- `compress()` / `decompress()` - Compress/decompress entire buffer

### Low-Level

- `Compressor` / `Decompressor` - Stateful packet-by-packet processing
- `count_encode()` / `count_decode()` - Counter encoding (Eq. 9)
- `rle_encode()` / `rle_decode()` - Run-length encoding (Eq. 10)
- `bit_extract()` / `bit_insert()` - Bit extraction (Eq. 11)

## References

- [CCSDS 124.0-B-1](https://ccsds.org/Pubs/124x0b1.pdf)
- [ESA POCKET+](https://opssat.esa.int/pocket-plus/)


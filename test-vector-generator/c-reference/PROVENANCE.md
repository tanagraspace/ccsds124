# Provenance

## Source

- **URL**: https://opssat.esa.int/pocket-plus/ (now defunct as of June 2026)
- **Obtained**: December 2, 2024
- **Description**: Barebone C reference implementation provided by ESA

## Original Archive

The source files in this directory were extracted from:

```
Filename: pocket-plus-master.tar.gz
MD5:      a31762a950de9465f9ec9964058b900d
SHA256:   c99eb51f90e2cfd1a09f881e78027ba08f352d646b3a03f72cc62072b3e71ea2
```

## Files Included

From the original archive, we extracted:
- `pocket_compress.c` - Compression implementation (48 KB)
- `pocket_decompress.c` - Decompression implementation (30 KB)
- `Makefile` - Build configuration
- `README.md` - Original ESA documentation

## Venus Express Test Data

The original archive also contained:
- `1028packets.ccsds` - Venus Express ADCS telemetry data (13.6 MB)
  - Now located at: `../../test-vectors/input/venus-express.ccsds`
  - Contains 1 week's worth of ADCS packets (ID 1028) from Venus Express
  - 151,200 packets × 90 bytes each
  - MD5: ac0ce25d660efa5ee18f92b71aa9a85d

## Documentation

The archive also contained documentation (not included in this repository):
- CCSDS 124.0-B-1 standard PDF
- Implementation papers
- PowerPoint presentations

See the main README for links to these resources online.

# Changelog

All notable changes to the C++ implementation are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
Releases are tagged `cpp/vX.Y.Z`.

## [1.0.0] - 2026-06-07

### Added

- Complete CCSDS 124.0-B-1 POCKET+ compression and decompression
- Byte-for-byte validation against the ESA reference implementation (all shared test vectors)
- CLI tool for compress/decompress operations

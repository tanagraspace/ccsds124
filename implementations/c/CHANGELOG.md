# Changelog

All notable changes to the C implementation are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
Releases are tagged `c/vX.Y.Z`.

## [1.0.0] - Unreleased

### Added

- Complete CCSDS 124.0-B-1 POCKET+ compression and decompression
- Byte-for-byte validation against the ESA reference implementation (all shared test vectors)
- CLI tool for compress/decompress operations
- `pocket_decompress_packet_checked()` — single-packet decompression with accuracy guarantee tracking (mask synchronization, status history, state save/restore, guarantee decision tree)
- `pocket_discover_packet_length()` — discover F from a compressed packet bitstream, with signaled-length validity rules (range 1–65535, RLE span consistency) and truncated-reference signaling via `POCKET_STATUS_TRUNCATED_LENGTH`
- Bitstream integrity validation in the decoder: underflow detection, RLE delta bounds checking, post-decompression padding verification (GOTCHAS.md #21)
- Reference packets (`rt=1`) tolerate excess trailing bits in checked decompression (self-delimiting via `COUNT(F)`); compressed packets (`rt=0`) keep the strict ≤7-padding-bits rule
- MISRA-C:2012 compliance, fuzzing harnesses, and CCSDS cross-validation harness (UAB suite)

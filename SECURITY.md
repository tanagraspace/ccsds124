# Security Policy

## Reporting a Vulnerability

Please report security vulnerabilities **privately** — do not open a public issue.

- Preferred: GitHub's private vulnerability reporting — open the repository's **Security** tab and choose **"Report a vulnerability"** ([Security Advisories](https://github.com/tanagraspace/ccsds124/security/advisories)).
- Alternatively, email **georges@tanagraspace.com** with details and, ideally, a minimal reproducer.

We will acknowledge your report and keep you updated as we investigate and remediate. Once a fix is available we will coordinate disclosure and credit reporters who wish to be named.

## Supported Versions

All six implementations are released together at the same version. Security fixes are applied to the latest released line only.

| Version | Supported |
|---------|-----------|
| 1.0.x   | ✅        |
| < 1.0   | ❌ (pre-release) |

## Scope and threat model

This project is a **lossless compression library** for fixed-length housekeeping telemetry (CCSDS 124.0-B-1). It has **no network, daemon, or privileged surface** — it compresses and decompresses byte buffers in-process. The relevant attack surface is therefore **processing untrusted or malformed input**, most importantly on the **decoder** (decompressing data received from a potentially lossy or hostile channel).

Posture across the implementations:

- **C** — uses static allocation only (no `malloc`/`free`), validates bitstream integrity to reject corrupt or truncated packets (see the [Implementer's Guide](docs/GOTCHAS.md) #20), and is checked with MISRA-C:2012, Valgrind, and continuous fuzzing (compress / decompress / round-trip harnesses, zero crashes observed).
- **C++** — header-only with zero dynamic allocation, analyzed with clang-tidy (CERT / HIC++ / C++ Core Guidelines).
- **Python, Go, Rust, Java** — memory-safe languages.

All six decoders **validate bitstream integrity** — corrupt or truncated input is rejected with an error rather than producing silent garbage (RLE-delta bounds and underflow checks). What is currently **C-only** is the higher-level *accuracy-guarantee* layer that decides whether a decompressed packet's output is reliable after packet loss or corruption; the other five languages do not yet make that determination. This is a correctness/feature gap, not memory unsafety, and is tracked in [#93](https://github.com/tanagraspace/ccsds124/issues/93). See [CONFORMANCE.md](docs/CONFORMANCE.md) for details.

In all cases, treat the output of decompressing untrusted input as untrusted until validated by your application.

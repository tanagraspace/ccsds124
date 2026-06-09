# Contributing to POCKET+

## Versioning Strategy

This monorepo uses **independent versioning** for each language implementation with prefixed Git tags:

- **C**: `c/v0.1.0`, `c/v0.2.0`, ...
- **C++**: `cpp/v0.1.0`, `cpp/v0.2.0`, ...
- **Python**: `python/v0.1.0`, `python/v0.2.0`, ...
- **Go**: `implementations/go/v0.1.0`, `implementations/go/v0.2.0`, ...
- **Rust**: `rust/v0.1.0`, `rust/v0.2.0`, ...
- **Java**: `java/v0.1.0`, `java/v0.2.0`, ...

> **Go exception:** the Go module lives at `implementations/go`, and the Go
> toolchain only resolves a version from a tag whose name is the module's
> path from the repo root followed by `/vX.Y.Z`. The tag must therefore be
> `implementations/go/vX.Y.Z` (not `go/vX.Y.Z`) for `go get ...@vX.Y.Z` to
> work. All tags keep the required `v` prefix.

Each implementation:
- Follows [Semantic Versioning](https://semver.org/)
- Maintains its own CHANGELOG.md
- Can be released independently
- Shares test vectors for validation

### Releasing

To release a new version of an implementation:

```bash
# Example: releasing Python v1.0.0
cd implementations/python
# Update version in pyproject.toml and pocketplus/__init__.py, then update CHANGELOG.md
git add pyproject.toml pocketplus/__init__.py CHANGELOG.md
git commit -m "python: release v1.0.0"
git tag python/v1.0.0
git push origin python/v1.0.0
```

## Documentation

- [Algorithm Specification](docs/ALGORITHM.md) - POCKET+ algorithm details
- [Implementation Guidelines](docs/GUIDELINES.md) - Quick start for implementers
- [Common Gotchas](docs/GOTCHAS.md) - Critical pitfalls to avoid
- [Test Vectors](test-vectors/README.md) - Validation test data

## Features

- **Lossless Compression** - Perfect reconstruction of original data
- **Delta Compression** - Efficient encoding of slowly-varying data
- **Packet Loss Resilience** - Decompressor continues even with lost packets
- **Low Complexity** - Uses only basic bitwise operations
- **Real-time Capable** - Suitable for time-critical spacecraft systems

## Interoperability

All implementations are designed to be interoperable:
- Data compressed by one implementation can be decompressed by others
- Shared test vectors ensure consistency
- Common documentation and specifications

## How to Contribute

Contributions are welcome! Please:

1. Read the [Implementation Guide](docs/GUIDELINES.md)
2. Ensure your changes pass all test vectors
3. Update relevant CHANGELOG.md
4. Follow language-specific conventions
5. Add tests for new functionality

## License

See [LICENSE](LICENSE) for details.

## References

- [POCKET+ on OPS-SAT-1 (SmallSat 2022)](https://digitalcommons.usu.edu/smallsat/2022/all2022/133/)
- [CCSDS 124.0-B-1 - Lossless Data Compression](https://ccsds.org/Pubs/124x0b1.pdf)
- [CCSDS Standards](https://ccsds.org/)

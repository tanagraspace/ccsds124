# CCSDS 124.0-B-1 Rust Implementation Makefile
# Wrapper around cargo commands for consistency with other implementations

BUILD_DIR = build
DOCS_DIR = $(BUILD_DIR)/docs

.PHONY: all build test test-cli test-report bench coverage clean fmt fmt-check clippy audit deny docs

all: build

build:
	cargo build --release

test:
	cargo test --release

test-cli: build
	./tests/test_cli.sh

test-report:
	@mkdir -p $(DOCS_DIR)/tests
	@echo "Running tests and generating report..."
	@cargo test 2>&1 | tee $(BUILD_DIR)/test-output.txt
	@../../scripts/generate-test-report.sh $(BUILD_DIR)/test-output.txt $(DOCS_DIR)/tests/index.html "CCSDS 124.0-B-1 Rust Test Report"

bench:
	cargo build --release --bin bench
	./target/release/bench

coverage:
	mkdir -p $(DOCS_DIR)/coverage
	cargo tarpaulin --out Html --output-dir $(DOCS_DIR)/coverage --exclude-files "src/bin/*" --exclude-files "tests/*"
	@mv $(DOCS_DIR)/coverage/tarpaulin-report.html $(DOCS_DIR)/coverage/index.html 2>/dev/null || true

fmt:
	cargo fmt

fmt-check:
	cargo fmt --check

clippy:
	cargo clippy -- -D warnings -W clippy::pedantic

audit:
	cargo audit

deny:
	cargo deny check

docs: test-report api-docs
	@echo "Documentation generated in $(DOCS_DIR)/"

api-docs:
	mkdir -p $(DOCS_DIR)
	cargo doc --no-deps --document-private-items
	cp -r target/doc $(DOCS_DIR)/api

clean:
	cargo clean
	rm -rf $(BUILD_DIR)/ 2>/dev/null || true

# Run all checks (for local development)
check: fmt-check clippy test

# Run all security checks
security: audit deny

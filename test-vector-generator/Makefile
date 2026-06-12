# Makefile for CCSDS 124.0-B-1 Test Vector Generation
# Provides convenient commands for building and running the test vector generator

.PHONY: help build generate clean verify copy-to-repo shell test

# Default target
help:
	@echo "CCSDS 124.0-B-1 Test Vector Generator"
	@echo ""
	@echo "Available targets:"
	@echo "  build          - Build the Docker image"
	@echo "  generate       - Generate all test vectors (builds if needed)"
	@echo "  generate-TYPE  - Generate specific test vector (simple, housekeeping, edge-cases, hiro, venus-express)"
	@echo "  verify         - Verify all generated test vectors"
	@echo "  copy-to-repo   - Copy generated vectors to ../test-vectors/"
	@echo "  clean          - Remove generated output files"
	@echo "  clean-all      - Remove output and Docker images"
	@echo "  shell          - Open a shell in the container"
	@echo "  test           - Run a quick test with simple vector"
	@echo ""
	@echo "Examples:"
	@echo "  make build"
	@echo "  make generate"
	@echo "  make generate-simple"
	@echo "  make test"

# Build the Docker image
build:
	@echo "Building Docker image..."
	docker-compose build

# Generate all test vectors
generate: build
	@echo "Generating all test vectors..."
	docker-compose run --rm test-vector-generator

# Generate specific test vectors
generate-simple: build
	@echo "Generating simple test vector..."
	docker-compose run --rm -e VECTOR_TYPE=simple test-vector-generator

generate-housekeeping: build
	@echo "Generating housekeeping test vector..."
	docker-compose run --rm -e VECTOR_TYPE=housekeeping test-vector-generator

generate-edge-cases: build
	@echo "Generating edge-cases test vector..."
	docker-compose run --rm -e VECTOR_TYPE=edge-cases test-vector-generator

generate-hiro: build
	@echo "Generating hiro (high robustness R=7) test vector..."
	docker-compose run --rm -e VECTOR_TYPE=hiro test-vector-generator

generate-venus-express: build
	@echo "Generating venus-express test vector (largest - runs last)..."
	docker-compose run --rm -e VECTOR_TYPE=venus-express test-vector-generator

# Verify generated test vectors
verify:
	@echo "Verifying test vectors..."
	@if [ -d output/vectors ]; then \
		for dir in output/vectors/*/; do \
			if [ -f "$$dir/metadata.json" ]; then \
				echo "Checking $$dir..."; \
				cat "$$dir/metadata.json" | grep -q '"roundtrip_verified": true' && echo "✓ OK" || echo "✗ FAILED"; \
			fi \
		done \
	else \
		echo "No test vectors found. Run 'make generate' first."; \
	fi

# Copy generated vectors to repository
copy-to-repo: generate
	@echo "Copying test vectors to repository..."
	docker-compose run --rm \
		-v $(PWD)/../test-vectors:/repo/test-vectors \
		test-vector-generator \
		/workspace/scripts/copy-to-repo.sh

# Clean generated output
clean:
	@echo "Cleaning generated output..."
	rm -rf output/vectors/*/
	@echo "Done."

# Clean everything including Docker images
clean-all: clean
	@echo "Removing Docker images..."
	docker-compose down --rmi all
	@echo "Done."

# Open a shell in the container
shell: build
	@echo "Opening shell in container..."
	docker-compose run --rm --entrypoint /bin/bash test-vector-generator

# Quick test with simple vector
test: generate-simple
	@echo ""
	@echo "Quick test completed!"
	@if [ -f output/vectors/simple/metadata.json ]; then \
		echo "Results:"; \
		cat output/vectors/simple/metadata.json | grep -E '"compression_ratio"|"roundtrip_verified"'; \
	fi

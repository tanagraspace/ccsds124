# CCSDS 124.0-B-1 C++ Implementation Makefile
# Wrapper for CMake build system

BUILD_DIR := build
DOCS_DIR := $(BUILD_DIR)/docs
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release

.PHONY: all build test test-report coverage coverage-html clean docs fmt fmt-check bench valgrind help

all: build

# Build the library and CLI
build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) ..
	@cmake --build $(BUILD_DIR) -- -j1

# Build with debug symbols
debug:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug ..
	@cmake --build $(BUILD_DIR) -- -j1

# Build for embedded (no exceptions, no RTTI)
embedded:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) -DCCSDS124_NO_EXCEPTIONS=ON -DCCSDS124_NO_RTTI=ON ..
	@cmake --build $(BUILD_DIR) -- -j1

# Run tests
test: build
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Run tests with HTML report
test-report: build
	@mkdir -p $(DOCS_DIR)/tests/html
	@$(MAKE) test 2>&1 | tee $(BUILD_DIR)/test-output.txt
	@../../scripts/generate-test-report.sh $(BUILD_DIR)/test-output.txt $(DOCS_DIR)/tests/html/index.html "CCSDS 124.0-B-1 C++ Test Report"

# Run tests with coverage (text summary)
coverage:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DCCSDS124_COVERAGE=ON ..
	@cmake --build $(BUILD_DIR) -- -j1
	@cd $(BUILD_DIR) && ctest --output-on-failure
	@lcov --capture --directory $(BUILD_DIR) --output-file $(BUILD_DIR)/coverage.info --ignore-errors source,gcov
	@lcov --remove $(BUILD_DIR)/coverage.info '/usr/*' '*/Catch2/*' '*/_deps/*' '*/tests/*' '*/src/cli.cpp' '*/bench/*' --output-file $(BUILD_DIR)/coverage.info --ignore-errors unused
	@echo ""
	@echo "Coverage Summary:"
	@echo "================="
	@lcov --summary $(BUILD_DIR)/coverage.info 2>&1 | grep -E "(lines|functions)"

# Generate HTML coverage report
coverage-html: coverage
	@mkdir -p $(DOCS_DIR)/coverage/html
	@cp $(BUILD_DIR)/coverage.info $(DOCS_DIR)/coverage/coverage.info
	@genhtml $(BUILD_DIR)/coverage.info --output-directory $(DOCS_DIR)/coverage/html
	@echo "Coverage data: $(DOCS_DIR)/coverage/coverage.info"
	@echo "Coverage HTML: $(DOCS_DIR)/coverage/html/index.html"

# Run benchmarks
bench: build
	@$(BUILD_DIR)/bench

# Format code with clang-format
fmt:
	@find include src tests bench -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i

# Check formatting
fmt-check:
	@find include src tests bench -name '*.hpp' -o -name '*.cpp' | xargs clang-format --dry-run --Werror

# Run clang-tidy
lint: build
	@find src -name '*.cpp' | xargs clang-tidy -p $(BUILD_DIR)

# Generate Doxygen documentation
docs: build
	@mkdir -p $(DOCS_DIR)/api
	@doxygen Doxyfile
	@echo "API documentation: $(DOCS_DIR)/api/index.html"

# Clean build artifacts
clean:
	@rm -rf $(BUILD_DIR)

# CLI tests
test-cli: build
	@./tests/test_cli.sh

# Run valgrind memory check
valgrind: build
	@command -v valgrind >/dev/null 2>&1 || { echo "Error: valgrind not installed"; exit 1; }
	@echo "Running valgrind memory check..."
	@mkdir -p $(DOCS_DIR)/valgrind
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 \
		$(BUILD_DIR)/tests 2>&1 | tee $(DOCS_DIR)/valgrind/report.txt
	@echo ""
	@echo "Valgrind report: $(DOCS_DIR)/valgrind/report.txt"
	@echo "Memory check passed - no leaks detected"

# Full check (CI equivalent)
check: fmt-check build test

help:
	@echo "CCSDS 124.0-B-1 C++ Build Targets:"
	@echo "  make build        - Build library and CLI"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make embedded     - Build without exceptions/RTTI"
	@echo "  make test         - Run unit tests"
	@echo "  make test-report  - Run tests with HTML report"
	@echo "  make test-cli     - Run CLI integration tests"
	@echo "  make coverage     - Run tests with coverage (text summary)"
	@echo "  make coverage-html- Generate HTML coverage report"
	@echo "  make valgrind     - Run valgrind memory check"
	@echo "  make bench        - Run benchmarks"
	@echo "  make fmt          - Format code"
	@echo "  make fmt-check    - Check formatting"
	@echo "  make lint         - Run clang-tidy"
	@echo "  make docs         - Generate documentation"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make check        - Full CI check"

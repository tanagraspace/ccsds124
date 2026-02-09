# POCKET+ C Implementation Makefile

CC = gcc
CFLAGS_BASE = -Wall -Wextra -std=c99 -Iinclude -pedantic -Werror -O3 -flto
CFLAGS ?= $(CFLAGS_BASE)
LDFLAGS = -flto

# For maximum performance on local machine, use: make CFLAGS="$(CFLAGS_BASE) -march=native"

# Coverage flags
COV_CFLAGS = $(CFLAGS) --coverage -fprofile-arcs -ftest-coverage
COV_LDFLAGS = $(LDFLAGS) --coverage

SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build
DOCS_DIR = $(BUILD_DIR)/docs
COV_DIR = $(BUILD_DIR)/coverage
CROSSVAL_DIR ?= crossvalidation
CROSSVAL_SCRIPT ?= $(CROSSVAL_DIR)/run_crossvalidation.sh

# Source files (exclude CLI which has main())
SRCS = $(filter-out $(SRC_DIR)/cli.c, $(wildcard $(SRC_DIR)/*.c))
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
COV_OBJS = $(SRCS:$(SRC_DIR)/%.c=$(COV_DIR)/%.o)

# Test files (exclude benchmarks from regular tests)
TEST_SRCS = $(filter-out $(TEST_DIR)/test_bench.c, $(wildcard $(TEST_DIR)/*.c))
TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)
COV_TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(COV_DIR)/%)

# Library and CLI
LIB = $(BUILD_DIR)/libpocketplus.a
CLI = $(BUILD_DIR)/pocketplus
COV_LIB = $(COV_DIR)/libpocketplus.a

# MISRA-C source files (excludes cli.c - ground tool, not flight software)
MISRA_SRCS = $(SRC_DIR)/bitvector.c $(SRC_DIR)/bitbuffer.c $(SRC_DIR)/compress.c \
             $(SRC_DIR)/decompress.c $(SRC_DIR)/encode.c $(SRC_DIR)/mask.c

# Cross-validation data directory (overridable)
CROSSVAL_DATA ?= ../../ccsds124_full_crossvalidation

# Cross-validation harness binaries
CROSSVAL_SRCS = $(wildcard $(CROSSVAL_DIR)/*.c)
CROSSVAL_BINS = $(CROSSVAL_SRCS:$(CROSSVAL_DIR)/%.c=$(BUILD_DIR)/%)

.PHONY: all clean test test-report test-cli test-reference cli coverage coverage-html docs misra valgrind bench crossvalidation

all: $(LIB) $(CLI)

cli: $(CLI)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(COV_DIR):
	mkdir -p $(COV_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(COV_DIR)/%.o: $(SRC_DIR)/%.c | $(COV_DIR)
	$(CC) $(COV_CFLAGS) -c $< -o $@

$(LIB): $(OBJS)
	ar rcs $@ $^

$(COV_LIB): $(COV_OBJS)
	ar rcs $@ $^

$(CLI): $(SRC_DIR)/cli.c $(LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lpocketplus $(LDFLAGS) -o $@

test: $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done

test-report: $(TEST_BINS)
	@mkdir -p $(DOCS_DIR)/tests/html
	@$(MAKE) test 2>&1 | tee $(BUILD_DIR)/test-output.txt
	@../../scripts/generate-test-report.sh $(BUILD_DIR)/test-output.txt $(DOCS_DIR)/tests/html/index.html "POCKET+ C Test Report"

test-cli: $(CLI)
	@$(TEST_DIR)/test_cli.sh

test-reference: $(CLI)
	@$(TEST_DIR)/test_reference.sh

bench: $(BUILD_DIR)/test_bench
	@$(BUILD_DIR)/test_bench

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lpocketplus $(LDFLAGS) -o $@

$(COV_DIR)/%: $(TEST_DIR)/%.c $(COV_LIB) | $(COV_DIR)
	$(CC) $(COV_CFLAGS) $< -L$(COV_DIR) -lpocketplus $(COV_LDFLAGS) -o $@

coverage: $(COV_TEST_BINS)
	@echo "Running tests with coverage..."
	@for test in $(COV_TEST_BINS); do \
		$$test || exit 1; \
	done
	@echo ""
	@echo "Coverage Summary:"
	@echo "================="
	@gcov -o $(COV_DIR) $(SRCS) 2>/dev/null | grep -A1 "^File.*src/" | grep -v "^--$$"
	@$(MAKE) --no-print-directory coverage-badge

coverage-badge:
	@mkdir -p assets
	@# Generate coverage.info first for accurate stats
	@lcov --capture --directory $(COV_DIR) --output-file $(COV_DIR)/coverage.info --include "*/src/*" 2>/dev/null || true
	@# Exclude CLI from coverage (tested via integration tests, not unit tests)
	@lcov --remove $(COV_DIR)/coverage.info '*/src/cli.c' --output-file $(COV_DIR)/coverage.info 2>/dev/null || true
	@# Extract line and function coverage from lcov summary
	@lcov --summary $(COV_DIR)/coverage.info 2>&1 | grep "lines" | sed 's/.*: //' | sed 's/%.*//' > $(COV_DIR)/.lines_pct
	@lcov --summary $(COV_DIR)/coverage.info 2>&1 | grep "functions" | sed 's/.*: //' | sed 's/%.*//' > $(COV_DIR)/.funcs_pct
	@# Generate lines badge
	@PCT=$$(cat $(COV_DIR)/.lines_pct); \
	if [ $$(echo "$$PCT >= 90" | bc) -eq 1 ]; then COLOR="brightgreen"; \
	elif [ $$(echo "$$PCT >= 80" | bc) -eq 1 ]; then COLOR="green"; \
	elif [ $$(echo "$$PCT >= 70" | bc) -eq 1 ]; then COLOR="yellowgreen"; \
	elif [ $$(echo "$$PCT >= 60" | bc) -eq 1 ]; then COLOR="yellow"; \
	else COLOR="red"; fi; \
	echo '<svg xmlns="http://www.w3.org/2000/svg" width="90" height="20">' > assets/coverage-lines.svg; \
	echo '<linearGradient id="b" x2="0" y2="100%"><stop offset="0" stop-color="#bbb" stop-opacity=".1"/><stop offset="1" stop-opacity=".1"/></linearGradient>' >> assets/coverage-lines.svg; \
	echo '<mask id="a"><rect width="90" height="20" rx="3" fill="#fff"/></mask>' >> assets/coverage-lines.svg; \
	echo '<g mask="url(#a)"><path fill="#555" d="M0 0h45v20H0z"/><path fill="'$$COLOR'" d="M45 0h45v20H45z"/><path fill="url(#b)" d="M0 0h90v20H0z"/></g>' >> assets/coverage-lines.svg; \
	echo '<g fill="#fff" text-anchor="middle" font-family="DejaVu Sans,Verdana,Geneva,sans-serif" font-size="11">' >> assets/coverage-lines.svg; \
	echo '<text x="22.5" y="15" fill="#010101" fill-opacity=".3">lines</text>' >> assets/coverage-lines.svg; \
	echo '<text x="22.5" y="14">lines</text>' >> assets/coverage-lines.svg; \
	echo '<text x="67" y="15" fill="#010101" fill-opacity=".3">'$$PCT'%</text>' >> assets/coverage-lines.svg; \
	echo '<text x="67" y="14">'$$PCT'%</text></g></svg>' >> assets/coverage-lines.svg; \
	echo "Badge: assets/coverage-lines.svg ($$PCT%)"
	@# Generate functions badge
	@PCT=$$(cat $(COV_DIR)/.funcs_pct); \
	if [ $$(echo "$$PCT >= 90" | bc) -eq 1 ]; then COLOR="brightgreen"; \
	elif [ $$(echo "$$PCT >= 80" | bc) -eq 1 ]; then COLOR="green"; \
	elif [ $$(echo "$$PCT >= 70" | bc) -eq 1 ]; then COLOR="yellowgreen"; \
	elif [ $$(echo "$$PCT >= 60" | bc) -eq 1 ]; then COLOR="yellow"; \
	else COLOR="red"; fi; \
	echo '<svg xmlns="http://www.w3.org/2000/svg" width="106" height="20">' > assets/coverage-functions.svg; \
	echo '<linearGradient id="b" x2="0" y2="100%"><stop offset="0" stop-color="#bbb" stop-opacity=".1"/><stop offset="1" stop-opacity=".1"/></linearGradient>' >> assets/coverage-functions.svg; \
	echo '<mask id="a"><rect width="106" height="20" rx="3" fill="#fff"/></mask>' >> assets/coverage-functions.svg; \
	echo '<g mask="url(#a)"><path fill="#555" d="M0 0h61v20H0z"/><path fill="'$$COLOR'" d="M61 0h45v20H61z"/><path fill="url(#b)" d="M0 0h106v20H0z"/></g>' >> assets/coverage-functions.svg; \
	echo '<g fill="#fff" text-anchor="middle" font-family="DejaVu Sans,Verdana,Geneva,sans-serif" font-size="11">' >> assets/coverage-functions.svg; \
	echo '<text x="30.5" y="15" fill="#010101" fill-opacity=".3">functions</text>' >> assets/coverage-functions.svg; \
	echo '<text x="30.5" y="14">functions</text>' >> assets/coverage-functions.svg; \
	echo '<text x="82.5" y="15" fill="#010101" fill-opacity=".3">'$$PCT'%</text>' >> assets/coverage-functions.svg; \
	echo '<text x="82.5" y="14">'$$PCT'%</text></g></svg>' >> assets/coverage-functions.svg; \
	echo "Badge: assets/coverage-functions.svg ($$PCT%)"

coverage-html:
	@command -v lcov >/dev/null 2>&1 || { echo "Error: lcov not installed. Install with: brew install lcov"; exit 1; }
	@mkdir -p $(DOCS_DIR)/coverage/html
	@if [ ! -f $(COV_DIR)/coverage.info ]; then \
		lcov --capture --directory $(COV_DIR) --output-file $(COV_DIR)/coverage.info --include "*/src/*"; \
		lcov --remove $(COV_DIR)/coverage.info '*/src/cli.c' --output-file $(COV_DIR)/coverage.info; \
	fi
	@cp $(COV_DIR)/coverage.info $(DOCS_DIR)/coverage/coverage.info
	genhtml $(COV_DIR)/coverage.info --output-directory $(DOCS_DIR)/coverage/html --ignore-errors inconsistent
	@echo "Coverage data: $(DOCS_DIR)/coverage/coverage.info"
	@echo "Coverage HTML: $(DOCS_DIR)/coverage/html/index.html"

docs:
	@command -v doxygen >/dev/null 2>&1 || { echo "Error: doxygen not installed. Install with: brew install doxygen"; exit 1; }
	@mkdir -p $(DOCS_DIR)/api
	@# Download Doxygen Awesome CSS if not present
	@if [ ! -f doxygen-awesome-css/doxygen-awesome.css ]; then \
		echo "Downloading Doxygen Awesome CSS theme..."; \
		mkdir -p doxygen-awesome-css; \
		curl -sL https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.3.4/doxygen-awesome.css -o doxygen-awesome-css/doxygen-awesome.css; \
		curl -sL https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.3.4/doxygen-awesome-sidebar-only.css -o doxygen-awesome-css/doxygen-awesome-sidebar-only.css; \
	fi
	doxygen Doxyfile
	@echo "API documentation: $(DOCS_DIR)/api/index.html"

misra:
	@command -v cppcheck >/dev/null 2>&1 || { echo "Error: cppcheck not installed. Install with: brew install cppcheck"; exit 1; }
	@mkdir -p $(BUILD_DIR)
	@echo "Running MISRA-C:2012 compliance check..."
	@cppcheck --addon=misra --suppressions-list=misra.supp \
		--enable=all -I$(INCLUDE_DIR) \
		--output-file=$(BUILD_DIR)/misra-report.txt \
		$(MISRA_SRCS) 2>&1
	@echo ""
	@echo "Report saved to $(BUILD_DIR)/misra-report.txt"
	@if grep -v "Unmatched suppression" $(BUILD_DIR)/misra-report.txt | grep -q "misra-c2012"; then \
		echo ""; \
		echo "ERROR: MISRA-C:2012 violations found:"; \
		grep -v "Unmatched suppression" $(BUILD_DIR)/misra-report.txt | grep "misra-c2012"; \
		exit 1; \
	fi

valgrind: $(TEST_BINS)
	@command -v valgrind >/dev/null 2>&1 || { echo "Error: valgrind not installed. Install with: brew install valgrind"; exit 1; }
	@echo "Running valgrind memory check..."
	@valgrind --leak-check=full --error-exitcode=1 $(BUILD_DIR)/test_vectors
	@echo ""
	@echo "Memory check passed - no leaks detected"

$(BUILD_DIR)/crossvalidation_%: $(CROSSVAL_DIR)/crossvalidation_%.c $(LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lpocketplus $(LDFLAGS) -o $@

# Usage: make crossvalidation [CROSSVAL_MODE=encoder|decoder|both]
CROSSVAL_MODE ?= both
CROSSVAL_RESULTS ?= $(BUILD_DIR)/crossvalidation-results.txt
crossvalidation: $(CROSSVAL_BINS)
	@ENCODER_BIN=$(BUILD_DIR)/crossvalidation_encoder \
	 DECODER_BIN=$(BUILD_DIR)/crossvalidation_decoder \
	 RESULTS_FILE=$(CROSSVAL_RESULTS) \
	 bash $(CROSSVAL_SCRIPT) $(CROSSVAL_MODE) $(CROSSVAL_DATA)

clean:
	rm -rf $(BUILD_DIR)/* $(BUILD_DIR)/.[!.]* 2>/dev/null || true

#!/bin/bash
# CCSDS 124.0-B-1 C++ CLI Integration Tests
#
# Tests the CLI tool for compression and decompression round-trips.
# Verifies that compressed output matches expected reference and
# decompressed output matches original input.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
PASSED=0
FAILED=0

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build"
CLI="${BUILD_DIR}/ccsds124"
TEST_VECTORS="/app/test-vectors"

# Check if running in Docker (test vectors at /app) or local
if [ ! -d "$TEST_VECTORS" ]; then
    TEST_VECTORS="${SCRIPT_DIR}/../../../test-vectors"
fi

# Temporary directory for test outputs
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Helper function to print test result
pass() {
    echo -e "${GREEN}PASS${NC}: $1"
    PASSED=$((PASSED + 1))
}

fail() {
    echo -e "${RED}FAIL${NC}: $1"
    FAILED=$((FAILED + 1))
}

skip() {
    echo -e "${YELLOW}SKIP${NC}: $1"
}

# Check if CLI exists
if [ ! -x "$CLI" ]; then
    echo "Error: CLI not found at $CLI"
    echo "Please build the project first: make build"
    exit 1
fi

echo "========================================"
echo "CCSDS 124.0-B-1 C++ CLI Integration Tests"
echo "========================================"
echo ""

# Test 1: Version flag
echo "Test 1: Version flag"
if $CLI --version 2>&1 | grep -q "ccsds124"; then
    pass "Version flag works"
else
    fail "Version flag"
fi

# Test 2: Help flag
echo "Test 2: Help flag"
if $CLI --help 2>&1 | grep -q "CCSDS 124.0-B-1"; then
    pass "Help flag works"
else
    fail "Help flag"
fi

# Test 3: No arguments shows help
echo "Test 3: No arguments shows usage"
if $CLI 2>&1 | grep -q "Usage"; then
    pass "No arguments shows usage"
else
    fail "No arguments shows usage"
fi

# Test 4: Invalid argument count
echo "Test 4: Invalid argument count"
if ! $CLI input.bin 90 2>&1 | grep -q "Error"; then
    fail "Invalid argument count not detected"
else
    pass "Invalid argument count detected"
fi

# Test 5: Round-trip test with simple vector
echo "Test 5: Round-trip with simple.bin"
if [ -f "${TEST_VECTORS}/input/simple.bin" ]; then
    cp "${TEST_VECTORS}/input/simple.bin" "$TEMP_DIR/"

    # Compress
    if $CLI "$TEMP_DIR/simple.bin" 90 10 20 50 1 > /dev/null 2>&1; then
        # Compare with expected output
        if [ -f "${TEST_VECTORS}/expected-output/simple.bin.pkt" ]; then
            if cmp -s "$TEMP_DIR/simple.bin.pkt" "${TEST_VECTORS}/expected-output/simple.bin.pkt"; then
                pass "Compression output matches reference"
            else
                fail "Compression output differs from reference"
            fi
        else
            skip "No reference output to compare"
        fi

        # Decompress
        if $CLI -d "$TEMP_DIR/simple.bin.pkt" 90 1 > /dev/null 2>&1; then
            # Compare with original
            if cmp -s "$TEMP_DIR/simple.bin.depkt" "${TEST_VECTORS}/input/simple.bin"; then
                pass "Round-trip: decompressed matches original"
            else
                fail "Round-trip: decompressed differs from original"
            fi
        else
            fail "Decompression failed"
        fi
    else
        fail "Compression failed"
    fi
else
    skip "simple.bin not found"
fi

# Test 6: Round-trip test with housekeeping vector (R=2)
echo "Test 6: Round-trip with housekeeping.bin (R=2)"
if [ -f "${TEST_VECTORS}/input/housekeeping.bin" ]; then
    cp "${TEST_VECTORS}/input/housekeeping.bin" "$TEMP_DIR/"

    # Compress
    if $CLI "$TEMP_DIR/housekeeping.bin" 90 20 50 100 2 > /dev/null 2>&1; then
        # Compare with expected output
        if [ -f "${TEST_VECTORS}/expected-output/housekeeping.bin.pkt" ]; then
            if cmp -s "$TEMP_DIR/housekeeping.bin.pkt" "${TEST_VECTORS}/expected-output/housekeeping.bin.pkt"; then
                pass "Compression output matches reference (R=2)"
            else
                fail "Compression output differs from reference (R=2)"
            fi
        fi

        # Decompress
        if $CLI -d "$TEMP_DIR/housekeeping.bin.pkt" 90 2 > /dev/null 2>&1; then
            if cmp -s "$TEMP_DIR/housekeeping.bin.depkt" "${TEST_VECTORS}/input/housekeeping.bin"; then
                pass "Round-trip: decompressed matches original (R=2)"
            else
                fail "Round-trip: decompressed differs from original (R=2)"
            fi
        else
            fail "Decompression failed (R=2)"
        fi
    else
        fail "Compression failed (R=2)"
    fi
else
    skip "housekeeping.bin not found"
fi

# Test 7: Round-trip test with hiro vector (R=7)
echo "Test 7: Round-trip with hiro.bin (R=7)"
if [ -f "${TEST_VECTORS}/input/hiro.bin" ]; then
    cp "${TEST_VECTORS}/input/hiro.bin" "$TEMP_DIR/"

    # Compress
    if $CLI "$TEMP_DIR/hiro.bin" 90 10 20 50 7 > /dev/null 2>&1; then
        # Compare with expected output
        if [ -f "${TEST_VECTORS}/expected-output/hiro.bin.pkt" ]; then
            if cmp -s "$TEMP_DIR/hiro.bin.pkt" "${TEST_VECTORS}/expected-output/hiro.bin.pkt"; then
                pass "Compression output matches reference (R=7)"
            else
                fail "Compression output differs from reference (R=7)"
            fi
        fi

        # Decompress
        if $CLI -d "$TEMP_DIR/hiro.bin.pkt" 90 7 > /dev/null 2>&1; then
            if cmp -s "$TEMP_DIR/hiro.bin.depkt" "${TEST_VECTORS}/input/hiro.bin"; then
                pass "Round-trip: decompressed matches original (R=7)"
            else
                fail "Round-trip: decompressed differs from original (R=7)"
            fi
        else
            fail "Decompression failed (R=7)"
        fi
    else
        fail "Compression failed (R=7)"
    fi
else
    skip "hiro.bin not found"
fi

# Test 8: Invalid robustness
echo "Test 8: Invalid robustness value"
if $CLI input.bin 90 10 20 50 10 2>&1 | grep -q "Error"; then
    pass "Invalid robustness detected"
else
    fail "Invalid robustness not detected"
fi

# Test 9: Invalid packet size
echo "Test 9: Invalid packet size"
if $CLI input.bin 0 10 20 50 1 2>&1 | grep -q "Error"; then
    pass "Invalid packet size detected"
else
    fail "Invalid packet size not detected"
fi

# Test 10: Decompress output filename
echo "Test 10: Decompress output filename removes .pkt"
if [ -f "$TEMP_DIR/simple.bin.pkt" ]; then
    # Check that .depkt file was created (not .pkt.depkt)
    if [ -f "$TEMP_DIR/simple.bin.depkt" ] && [ ! -f "$TEMP_DIR/simple.bin.pkt.depkt" ]; then
        pass "Decompress output filename correct (.pkt removed)"
    else
        fail "Decompress output filename incorrect"
    fi
else
    skip "No compressed file to test"
fi

echo ""
echo "========================================"
echo "Results: ${PASSED} passed, ${FAILED} failed"
echo "========================================"

if [ $FAILED -gt 0 ]; then
    exit 1
fi

exit 0

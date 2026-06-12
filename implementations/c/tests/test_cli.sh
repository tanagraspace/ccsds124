#!/bin/sh
# CLI round-trip tests for CCSDS 124.0-B-1 compress/decompress
#
# Tests the CLI executable by compressing test vectors,
# decompressing them, and verifying the output matches the original.

set -e

CLI="./build/ccsds124"
TEST_VECTORS_DIR="../../test-vectors/input"
TEMP_DIR="$(mktemp -d)"
TESTS_RUN=0
TESTS_PASSED=0

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

# Hash function (works on both Linux and macOS)
compute_hash() {
    if command -v md5sum >/dev/null 2>&1; then
        md5sum "$1" | cut -d' ' -f1
    else
        md5 -q "$1"
    fi
}

test_roundtrip() {
    name="$1"
    input="$2"
    packet_size="$3"
    pt="$4"
    ft="$5"
    rt="$6"
    robustness="$7"

    TESTS_RUN=$((TESTS_RUN + 1))
    printf "  %s..." "$name"

    # Copy input to temp dir
    cp "$input" "$TEMP_DIR/input.bin"

    # Compress
    "$CLI" "$TEMP_DIR/input.bin" "$packet_size" "$pt" "$ft" "$rt" "$robustness" >/dev/null 2>&1

    # Verify compressed file exists
    if [ ! -f "$TEMP_DIR/input.bin.pkt" ]; then
        echo " FAIL (compression failed)"
        return 1
    fi

    # Decompress (CLI strips .pkt and adds .depkt)
    "$CLI" -d "$TEMP_DIR/input.bin.pkt" "$packet_size" "$robustness" >/dev/null 2>&1

    # Verify decompressed file exists (output is input.bin.depkt)
    if [ ! -f "$TEMP_DIR/input.bin.depkt" ]; then
        echo " FAIL (decompression failed)"
        return 1
    fi

    # Compare hashes
    original_hash=$(compute_hash "$TEMP_DIR/input.bin")
    roundtrip_hash=$(compute_hash "$TEMP_DIR/input.bin.depkt")

    if [ "$original_hash" = "$roundtrip_hash" ]; then
        input_size=$(wc -c < "$input" | tr -d ' ')
        comp_size=$(wc -c < "$TEMP_DIR/input.bin.pkt" | tr -d ' ')
        ratio=$(echo "scale=2; $input_size / $comp_size" | bc)
        echo " OK (${input_size}B -> ${comp_size}B, ${ratio}x)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo " FAIL (hash mismatch)"
        echo "    Original:  $original_hash"
        echo "    Roundtrip: $roundtrip_hash"
        return 1
    fi

    # Clean up for next test
    rm -f "$TEMP_DIR"/*
}

echo ""
echo "CLI Round-trip Tests"
echo "===================="
echo ""

# Check CLI exists
if [ ! -x "$CLI" ]; then
    echo "ERROR: CLI not found at $CLI"
    echo "Run 'make' first to build the CLI."
    exit 1
fi

# Test vectors with their parameters
# Format: name, input_file, packet_size, pt, ft, rt, robustness

test_roundtrip "simple" \
    "$TEST_VECTORS_DIR/simple.bin" \
    90 10 20 50 1

test_roundtrip "housekeeping" \
    "$TEST_VECTORS_DIR/housekeeping.bin" \
    90 20 50 100 2

test_roundtrip "edge-cases" \
    "$TEST_VECTORS_DIR/edge-cases.bin" \
    90 10 20 50 1

test_roundtrip "venus-express" \
    "$TEST_VECTORS_DIR/venus-express.ccsds" \
    90 20 50 100 2

echo ""
echo "Results: $TESTS_PASSED/$TESTS_RUN tests passed"
echo ""

if [ "$TESTS_PASSED" -ne "$TESTS_RUN" ]; then
    echo "FAIL: Some CLI tests failed"
    exit 1
fi

echo "All CLI tests passed!"
exit 0

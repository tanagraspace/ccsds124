#!/bin/bash
#
# Test our implementation against ESA C reference for all R values (0-7)
#
# Validates bit-identical compressed output for each robustness level.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMPL_DIR="$(dirname "$SCRIPT_DIR")"
REPO_ROOT="$(dirname "$(dirname "$IMPL_DIR")")"
REF_DIR="$REPO_ROOT/test-vector-generator/c-reference"
OUR_CLI="$IMPL_DIR/build/ccsds124"
REF_COMPRESS="$REF_DIR/ccsds124_compress"
REF_DECOMPRESS="$REF_DIR/ccsds124_decompress"
TMP_DIR=$(mktemp -d)

# Cleanup on exit
trap "rm -rf $TMP_DIR" EXIT

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo ""
echo "========================================"
echo "Reference Implementation Validation"
echo "========================================"
echo ""

# Check prerequisites
if [ ! -x "$OUR_CLI" ]; then
    echo -e "${RED}ERROR: Our CLI not found at $OUR_CLI${NC}"
    echo "Run 'make' first to build."
    exit 1
fi

if [ ! -x "$REF_COMPRESS" ]; then
    echo -e "${RED}ERROR: Reference compressor not found at $REF_COMPRESS${NC}"
    echo "Build reference with: cd $REF_DIR && make"
    exit 1
fi

if [ ! -x "$REF_DECOMPRESS" ]; then
    echo -e "${RED}ERROR: Reference decompressor not found at $REF_DECOMPRESS${NC}"
    exit 1
fi

echo "Our CLI:      $OUR_CLI"
echo "Reference:    $REF_COMPRESS"
echo "Temp dir:     $TMP_DIR"
echo ""

# Test parameters
PACKET_LENGTH=90
PT=10
FT=20
RT=50
NUM_PACKETS=100
INPUT_SIZE=$((PACKET_LENGTH * NUM_PACKETS))

PASS=0
FAIL=0

# Generate test input (housekeeping-like pattern)
# Note: Random/high-entropy data can exceed reference's 4x buffer limit
# Using realistic housekeeping pattern: mostly stable with small variations
echo "Generating test input: $INPUT_SIZE bytes ($NUM_PACKETS packets)..."
python3 -c "
import sys
data = bytearray()
for pkt in range($NUM_PACKETS):
    for byte_idx in range($PACKET_LENGTH):
        if byte_idx < 4:  # Timestamp-like field (changes each packet)
            data.append((pkt + byte_idx) % 256)
        else:
            data.append(0x55)  # Stable telemetry
sys.stdout.buffer.write(bytes(data))
" > "$TMP_DIR/input.bin"
echo ""

echo "Testing all R values (0-7) against reference..."
echo ""

for R in 0 1 2 3 4 5 6 7; do
    printf "  R=%d: " "$R"

    # Compress with reference
    cp "$TMP_DIR/input.bin" "$TMP_DIR/ref_input.bin"
    (cd "$TMP_DIR" && "$REF_COMPRESS" ref_input.bin $PACKET_LENGTH $PT $FT $RT $R > /dev/null 2>&1)
    REF_COMPRESSED="$TMP_DIR/ref_input.bin.pkt"

    if [ ! -f "$REF_COMPRESSED" ]; then
        echo -e "${RED}FAIL${NC} - Reference compression failed"
        FAIL=$((FAIL + 1))
        continue
    fi

    # Compress with our implementation
    cp "$TMP_DIR/input.bin" "$TMP_DIR/our_input.bin"
    (cd "$TMP_DIR" && "$OUR_CLI" our_input.bin $PACKET_LENGTH $PT $FT $RT $R > /dev/null 2>&1)
    OUR_COMPRESSED="$TMP_DIR/our_input.bin.pkt"

    if [ ! -f "$OUR_COMPRESSED" ]; then
        echo -e "${RED}FAIL${NC} - Our compression failed"
        FAIL=$((FAIL + 1))
        continue
    fi

    # Compare compressed output
    REF_SIZE=$(stat -f%z "$REF_COMPRESSED" 2>/dev/null || stat -c%s "$REF_COMPRESSED")
    OUR_SIZE=$(stat -f%z "$OUR_COMPRESSED" 2>/dev/null || stat -c%s "$OUR_COMPRESSED")

    if cmp -s "$REF_COMPRESSED" "$OUR_COMPRESSED"; then
        echo -e "${GREEN}PASS${NC} - Bit-identical ($OUR_SIZE bytes)"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC} - Output differs (ref: $REF_SIZE bytes, ours: $OUR_SIZE bytes)"
        FAIL=$((FAIL + 1))

        # Show first difference
        echo "    First difference at byte:"
        cmp "$REF_COMPRESSED" "$OUR_COMPRESSED" 2>&1 | head -1 || true
    fi

    # Cleanup for next iteration
    rm -f "$TMP_DIR/ref_input.bin.pkt" "$OUR_COMPRESSED" "$TMP_DIR/our_input.bin"
done

echo ""
echo "========================================"
echo "Results: $PASS/8 R values match reference"
echo "========================================"
echo ""

if [ $FAIL -gt 0 ]; then
    exit 1
fi

exit 0

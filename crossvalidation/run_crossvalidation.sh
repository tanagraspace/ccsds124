#!/usr/bin/env bash
# CCSDS 124.0-B-1 Cross-Validation Runner
#
# Runs encoder and decoder harnesses against all test vectors,
# validates generated output files against file_list.csv (size + SHA-256).
#
# Usage: ./run_crossvalidation.sh [encoder|decoder|both] [crossvalidation_data_dir]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FILE_LIST="${SCRIPT_DIR}/file_list.csv"

# Parse arguments: [encoder|decoder|both] [crossvalidation_data_dir]
RUN_MODE="both"
CROSSVAL_DATA=""
for arg in "$@"; do
    case "$arg" in
        encoder|decoder|both) RUN_MODE="$arg" ;;
        *) CROSSVAL_DATA="$arg" ;;
    esac
done
CROSSVAL_DATA="${CROSSVAL_DATA:-${SCRIPT_DIR}/../ccsds124_full_crossvalidation}"

# Resolve to absolute path
CROSSVAL_DATA="$(cd "$CROSSVAL_DATA" 2>/dev/null && pwd)" || {
    echo "Error: cross-validation data directory not found: $CROSSVAL_DATA"
    echo "Download and extract it first, e.g.:"
    echo "  unzip ccsds124_full_crossvalidation_20220309.zip -d ccsds124_full_crossvalidation"
    exit 1
}

# Verify required files exist
if [ ! -f "$FILE_LIST" ]; then
    echo "Error: file_list.csv not found at $FILE_LIST"
    exit 1
fi

if [ ! -d "$CROSSVAL_DATA/encoder_input" ]; then
    echo "Error: encoder_input directory not found in $CROSSVAL_DATA"
    exit 1
fi

if [ ! -d "$CROSSVAL_DATA/decoder_input" ]; then
    echo "Error: decoder_input directory not found in $CROSSVAL_DATA"
    exit 1
fi

# Results file (optional, defaults to crossvalidation-results.txt next to this script)
RESULTS_FILE="${RESULTS_FILE:-${SCRIPT_DIR}/crossvalidation-results.txt}"

# Known-failures baseline (optional, defaults to known-failures.txt next to this script).
# Failures listed in this file are documented gaps: the run passes when actual
# failures match the baseline exactly (no new failures). Known failures that now
# pass are reported so the baseline can be trimmed.
KNOWN_FAILURES="${KNOWN_FAILURES:-${SCRIPT_DIR}/known-failures.txt}"

# Binary paths must be provided via environment variables
ENCODER_BIN="${ENCODER_BIN:?Error: ENCODER_BIN environment variable not set}"
DECODER_BIN="${DECODER_BIN:?Error: DECODER_BIN environment variable not set}"

if [ ! -x "$ENCODER_BIN" ]; then
    echo "Error: encoder binary not found at $ENCODER_BIN"
    exit 1
fi

if [ ! -x "$DECODER_BIN" ]; then
    echo "Error: decoder binary not found at $DECODER_BIN"
    exit 1
fi

# Parse file_list.csv into associative arrays
declare -A EXPECTED_SIZE
declare -A EXPECTED_SHA256

while IFS=',' read -r path size sha256; do
    # Skip header
    [ "$path" = "path" ] && continue
    EXPECTED_SIZE["$path"]="$size"
    EXPECTED_SHA256["$path"]="$sha256"
done < "$FILE_LIST"

# Create temp directory for generated outputs
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

# Detect SHA-256 command
if command -v sha256sum &>/dev/null; then
    sha256cmd() { sha256sum "$1" | awk '{print $1}'; }
elif command -v shasum &>/dev/null; then
    sha256cmd() { shasum -a 256 "$1" | awk '{print $1}'; }
else
    echo "Error: neither sha256sum nor shasum found"
    exit 1
fi

# Logging helper: writes to both stdout and results file
log() { printf '%s\n' "$*" | tee -a "$RESULTS_FILE"; }

# Initialize results file
mkdir -p "$(dirname "$RESULTS_FILE")"
{
    echo "# CCSDS 124.0-B-1 Cross-Validation Results"
    echo "# Date: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    echo "# Encoder: ${ENCODER_BIN}"
    echo "# Decoder: ${DECODER_BIN}"
    echo "# Mode: ${RUN_MODE}"
    echo ""
} > "$RESULTS_FILE"

log "=== CCSDS 124.0-B-1 Cross-Validation ==="
log ""

total_pass=0
total_fail=0
failures=""
failure_names=""

# --- Encoder pass ---
encoder_pass=0
encoder_fail=0
encoder_count=0

if [ "$RUN_MODE" = "encoder" ] || [ "$RUN_MODE" = "both" ]; then
log "--- Encoder ---"
for input_file in "$CROSSVAL_DATA"/encoder_input/*.raw+config; do
    [ -f "$input_file" ] || continue
    encoder_count=$((encoder_count + 1))

    # Derive output filename
    basename=$(basename "$input_file")
    # encoder_sequence_XXXX.raw+config -> encoder_sequence_XXXX.124
    output_name="${basename%.raw+config}.124"
    output_file="${TMPDIR}/${output_name}"
    csv_key="/encoder_output/${output_name}"

    # Run encoder harness
    if ! "$ENCODER_BIN" "$input_file" "$output_file" 2>/dev/null; then
        encoder_fail=$((encoder_fail + 1))
        failures="${failures}  ENCODER CRASH: ${basename}\n"
        failure_names="${failure_names}${output_name}"$'\n'
        printf "\r[%d/%s] %s - CRASH" "$encoder_count" "?" "$output_name"
        continue
    fi

    # Get expected values from file_list.csv
    expected_size="${EXPECTED_SIZE[$csv_key]:-}"
    expected_sha="${EXPECTED_SHA256[$csv_key]:-}"

    if [ -z "$expected_size" ] || [ -z "$expected_sha" ]; then
        encoder_fail=$((encoder_fail + 1))
        failures="${failures}  ENCODER NOT IN CSV: ${output_name}\n"
        failure_names="${failure_names}${output_name}"$'\n'
        continue
    fi

    # Compute actual size and SHA-256
    if [ -f "$output_file" ]; then
        actual_size=$(wc -c < "$output_file" | tr -d ' ')
        actual_sha=$(sha256cmd "$output_file")
    else
        actual_size=0
        actual_sha=""
    fi

    # Compare
    if [ "$actual_size" = "$expected_size" ] && [ "$actual_sha" = "$expected_sha" ]; then
        encoder_pass=$((encoder_pass + 1))
    else
        encoder_fail=$((encoder_fail + 1))
        failures="${failures}  ENCODER FAIL: ${output_name} (size: ${actual_size}/${expected_size}, sha256: ${actual_sha:0:16}.../${expected_sha:0:16}...)\n"
        failure_names="${failure_names}${output_name}"$'\n'
    fi

    printf "\r[%d] %s    " "$encoder_count" "$output_name"
done

printf "\r"
log "Encoder: ${encoder_pass} passed, ${encoder_fail} failed (of ${encoder_count})"
log ""

total_pass=$((total_pass + encoder_pass))
total_fail=$((total_fail + encoder_fail))
fi

# --- Decoder pass ---
decoder_pass=0
decoder_fail=0
decoder_count=0

if [ "$RUN_MODE" = "decoder" ] || [ "$RUN_MODE" = "both" ]; then
log "--- Decoder ---"
for input_file in "$CROSSVAL_DATA"/decoder_input/*.124+config; do
    [ -f "$input_file" ] || continue
    decoder_count=$((decoder_count + 1))

    # Derive output filename
    basename=$(basename "$input_file")
    # decoder_sequence_XXXXX.124+config -> decoder_sequence_XXXXX.raw+large_f
    output_name="${basename%.124+config}.raw+large_f"
    output_file="${TMPDIR}/${output_name}"
    csv_key="/decoder_output/${output_name}"

    # Run decoder harness
    if ! "$DECODER_BIN" "$input_file" "$output_file" 2>/dev/null; then
        decoder_fail=$((decoder_fail + 1))
        failures="${failures}  DECODER CRASH: ${basename}\n"
        failure_names="${failure_names}${output_name}"$'\n'
        printf "\r[%d/%s] %s - CRASH" "$decoder_count" "?" "$output_name"
        continue
    fi

    # Get expected values from file_list.csv
    expected_size="${EXPECTED_SIZE[$csv_key]:-}"
    expected_sha="${EXPECTED_SHA256[$csv_key]:-}"

    if [ -z "$expected_size" ] || [ -z "$expected_sha" ]; then
        decoder_fail=$((decoder_fail + 1))
        failures="${failures}  DECODER NOT IN CSV: ${output_name}\n"
        failure_names="${failure_names}${output_name}"$'\n'
        continue
    fi

    # Compute actual size and SHA-256
    if [ -f "$output_file" ]; then
        actual_size=$(wc -c < "$output_file" | tr -d ' ')
        actual_sha=$(sha256cmd "$output_file")
    else
        actual_size=0
        actual_sha=""
    fi

    # Compare
    if [ "$actual_size" = "$expected_size" ] && [ "$actual_sha" = "$expected_sha" ]; then
        decoder_pass=$((decoder_pass + 1))
    else
        decoder_fail=$((decoder_fail + 1))
        failures="${failures}  DECODER FAIL: ${output_name} (size: ${actual_size}/${expected_size}, sha256: ${actual_sha:0:16}.../${expected_sha:0:16}...)\n"
        failure_names="${failure_names}${output_name}"$'\n'
    fi

    printf "\r[%d] %s    " "$decoder_count" "$output_name"
done

printf "\r"
log "Decoder: ${decoder_pass} passed, ${decoder_fail} failed (of ${decoder_count})"
log ""

total_pass=$((total_pass + decoder_pass))
total_fail=$((total_fail + decoder_fail))
fi

# --- Summary ---
if [ -n "$failures" ]; then
    log "Failures:"
    printf "$failures" | tee -a "$RESULTS_FILE"
    log ""
fi

log "=== Summary: ${total_pass} passed, ${total_fail} failed ==="

if [ "$total_fail" -eq 0 ]; then
    log ""
    log "Result: PASS"
    echo ""
    echo "Results saved to ${RESULTS_FILE}"
    exit 0
fi

# Compare failures against the known-failures baseline (documented gaps).
if [ -f "$KNOWN_FAILURES" ]; then
    # Restrict the baseline to the phases that ran in this invocation
    case "$RUN_MODE" in
        encoder) baseline_filter='^encoder_' ;;
        decoder) baseline_filter='^decoder_' ;;
        *)       baseline_filter='' ;;
    esac
    awk -v f="$baseline_filter" '/^[[:space:]]*(#|$)/ { next } f == "" || $0 ~ f' \
        "$KNOWN_FAILURES" | sort -u > "$TMPDIR/baseline.txt"
    printf '%s' "$failure_names" | sort -u > "$TMPDIR/actual.txt"

    new_failures=$(comm -23 "$TMPDIR/actual.txt" "$TMPDIR/baseline.txt")
    fixed_failures=$(comm -13 "$TMPDIR/actual.txt" "$TMPDIR/baseline.txt")

    if [ -z "$new_failures" ]; then
        log ""
        log "All ${total_fail} failures are documented gaps in $(basename "$KNOWN_FAILURES")."
        if [ -n "$fixed_failures" ]; then
            fixed_count=$(printf '%s\n' "$fixed_failures" | wc -l | tr -d ' ')
            log "${fixed_count} known failure(s) now pass - remove them from $(basename "$KNOWN_FAILURES"):"
            printf '%s\n' "$fixed_failures" | sed 's/^/  /' | tee -a "$RESULTS_FILE"
        fi
        log ""
        log "Result: PASS (matches known-failures baseline)"
        echo ""
        echo "Results saved to ${RESULTS_FILE}"
        exit 0
    fi

    new_count=$(printf '%s\n' "$new_failures" | wc -l | tr -d ' ')
    log ""
    log "${new_count} new failure(s) not in $(basename "$KNOWN_FAILURES"):"
    printf '%s\n' "$new_failures" | sed 's/^/  /' | tee -a "$RESULTS_FILE"
fi

log ""
log "Result: FAIL"
echo ""
echo "Results saved to ${RESULTS_FILE}"
exit 1

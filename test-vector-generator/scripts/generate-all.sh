#!/bin/bash
# Main orchestration script for generating all test vectors
# This script generates synthetic inputs, compresses them with the C reference,
# decompresses and verifies, then outputs metadata

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_DIR="/workspace"
CONFIGS_DIR="${WORKSPACE_DIR}/configs"
GENERATORS_DIR="${WORKSPACE_DIR}/input-generators"
OUTPUT_DIR="${WORKSPACE_DIR}/output"

echo "=========================================="
echo "CCSDS 124.0-B-1 Test Vector Generation"
echo "=========================================="
echo ""

# Determine which vectors to generate
VECTOR_TYPE="${VECTOR_TYPE:-all}"

generate_vector() {
    local name=$1
    local config="${CONFIGS_DIR}/${name}.yaml"

    echo "------------------------------------------"
    echo "Generating: ${name}"
    echo "------------------------------------------"

    if [ ! -f "${config}" ]; then
        echo "ERROR: Config not found: ${config}"
        return 1
    fi

    # Read config values using Python
    local input_type=$(python3 -c "import yaml; print(yaml.safe_load(open('${config}'))['input']['type'])")

    if [ "${input_type}" = "synthetic" ]; then
        # Generate synthetic input using Python generator
        echo "Step 1: Generating synthetic input data..."
        python3 "${GENERATORS_DIR}/generate_${name}.py" "${config}"
        echo ""
    else
        # Real data - just verify it exists
        echo "Step 1: Using real data (Venus Express)..."
        local source=$(python3 -c "import yaml; print(yaml.safe_load(open('${config}'))['input']['source'])")
        if [ ! -f "${source}" ]; then
            echo "ERROR: Source data not found: ${source}"
            return 1
        fi

        # Verify MD5
        local expected_md5=$(python3 -c "import yaml; print(yaml.safe_load(open('${config}'))['input']['expected_md5'])")
        local actual_md5=$(md5sum "${source}" | awk '{print $1}')

        if [ "${expected_md5}" != "${actual_md5}" ]; then
            echo "ERROR: MD5 mismatch for ${source}"
            echo "  Expected: ${expected_md5}"
            echo "  Actual:   ${actual_md5}"
            return 1
        fi
        echo "✓ MD5 verified: ${expected_md5}"
        echo ""
    fi

    # Run compression and verification
    echo "Step 2: Running compression and verification..."
    bash "${SCRIPT_DIR}/verify-vector.sh" "${name}" "${config}"
    echo ""

    echo "✓ ${name} completed successfully"
    echo ""
}

# Generate test vectors based on VECTOR_TYPE
if [ "${VECTOR_TYPE}" = "all" ]; then
    generate_vector "simple"
    generate_vector "housekeeping"
    generate_vector "edge-cases"
    generate_vector "venus-express"
else
    generate_vector "${VECTOR_TYPE}"
fi

echo "=========================================="
echo "All test vectors generated successfully!"
echo "=========================================="
echo ""
echo "Output directory: ${OUTPUT_DIR}/vectors"
echo ""
echo "Next steps:"
echo "  1. Review the generated test vectors"
echo "  2. Check metadata.json files for compression statistics"
echo "  3. Copy to repository using: make copy-to-repo"
echo ""

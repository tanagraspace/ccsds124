"""Test vectors validation against reference implementation.

Validates Python CCSDS 124.0-B-1 output matches byte-for-byte with
expected output from ESA reference implementation.

Run all tests:
    pytest tests/test_vectors.py

Skip slow tests (venus-express, housekeeping):
    pytest tests/test_vectors.py -m "not slow"
"""

import json
from pathlib import Path

import pytest

from ccsds124 import compress, decompress

# Path to test vectors (relative to repo root)
REPO_ROOT = Path(__file__).parent.parent.parent.parent
TEST_VECTORS_DIR = REPO_ROOT / "test-vectors"
INPUT_DIR = TEST_VECTORS_DIR / "input"
EXPECTED_DIR = TEST_VECTORS_DIR / "expected-output"

# Large vectors that take significant time to process
SLOW_VECTORS = {"venus-express", "housekeeping"}


def load_metadata(name: str) -> dict:
    """Load metadata JSON for a test vector."""
    metadata_path = EXPECTED_DIR / f"{name}-metadata.json"
    with open(metadata_path) as f:
        return json.load(f)


def get_test_vectors() -> list:
    """Get list of available test vectors with their parameters."""
    vectors = []

    # Map metadata input filenames to actual filenames (some were renamed)
    filename_map = {
        "1028packets.ccsds": "venus-express.ccsds",
    }

    # Find all metadata files
    for metadata_file in EXPECTED_DIR.glob("*-metadata.json"):
        name = metadata_file.stem.replace("-metadata", "")
        metadata = load_metadata(name)

        # Get input filename, applying remap if needed
        input_file = metadata["input"]["file"]
        input_file = filename_map.get(input_file, input_file)

        # Skip if input file doesn't exist
        if not (INPUT_DIR / input_file).exists():
            continue

        vectors.append(
            {
                "name": name,
                "input_file": input_file,
                "output_file": metadata["output"]["compressed"]["file"],
                "packet_length": metadata["compression"]["packet_length"],
                "robustness": metadata["compression"]["parameters"]["robustness"],
                "pt": metadata["compression"]["parameters"]["pt"],
                "ft": metadata["compression"]["parameters"]["ft"],
                "rt": metadata["compression"]["parameters"]["rt"],
                "expected_size": metadata["output"]["compressed"]["size"],
                "expected_md5": metadata["output"]["compressed"]["md5"],
            }
        )

    return vectors


# Get test vectors for parametrization
TEST_VECTORS = get_test_vectors()


def get_parametrized_vectors():
    """Get test vectors with slow markers applied."""
    params = []
    for v in TEST_VECTORS:
        if v["name"] in SLOW_VECTORS:
            params.append(pytest.param(v, marks=pytest.mark.slow, id=v["name"]))
        else:
            params.append(pytest.param(v, id=v["name"]))
    return params


PARAMETRIZED_VECTORS = get_parametrized_vectors()


@pytest.fixture
def vector_data(request):
    """Load input and expected output for a test vector."""
    vector = request.param

    input_path = INPUT_DIR / vector["input_file"]
    expected_path = EXPECTED_DIR / vector["output_file"]

    with open(input_path, "rb") as f:
        input_data = f.read()

    with open(expected_path, "rb") as f:
        expected_output = f.read()

    return {
        **vector,
        "input_data": input_data,
        "expected_output": expected_output,
    }


class TestVectorCompression:
    """Test compression output matches reference."""

    @pytest.mark.parametrize("vector_data", PARAMETRIZED_VECTORS, indirect=True)
    def test_compression_matches_reference(self, vector_data: dict) -> None:
        """Test that compression produces byte-identical output."""
        input_data = vector_data["input_data"]
        expected_output = vector_data["expected_output"]

        # Compress using Python implementation
        # packet_length is in bytes, packet_size is in bits
        packet_size_bits = vector_data["packet_length"] * 8

        actual_output = compress(
            input_data,
            packet_size=packet_size_bits,
            robustness=vector_data["robustness"],
            pt_limit=vector_data["pt"],
            ft_limit=vector_data["ft"],
            rt_limit=vector_data["rt"],
        )

        # Compare byte-for-byte
        assert actual_output == expected_output, (
            f"Compression output mismatch for {vector_data['name']}: "
            f"expected {len(expected_output)} bytes, got {len(actual_output)} bytes"
        )


class TestVectorRoundTrip:
    """Test round-trip decompression."""

    @pytest.mark.parametrize("vector_data", PARAMETRIZED_VECTORS, indirect=True)
    def test_round_trip(self, vector_data: dict) -> None:
        """Test that compress then decompress returns original."""
        input_data = vector_data["input_data"]
        packet_size_bits = vector_data["packet_length"] * 8

        # Compress
        compressed = compress(
            input_data,
            packet_size=packet_size_bits,
            robustness=vector_data["robustness"],
            pt_limit=vector_data["pt"],
            ft_limit=vector_data["ft"],
            rt_limit=vector_data["rt"],
        )

        # Decompress
        decompressed = decompress(
            compressed,
            packet_size=packet_size_bits,
            robustness=vector_data["robustness"],
        )

        assert decompressed == input_data, (
            f"Round-trip failed for {vector_data['name']}: "
            f"expected {len(input_data)} bytes, got {len(decompressed)} bytes"
        )


class TestVectorDecompressReference:
    """Test decompression of reference compressed output."""

    @pytest.mark.parametrize("vector_data", PARAMETRIZED_VECTORS, indirect=True)
    def test_decompress_reference_output(self, vector_data: dict) -> None:
        """Test that decompressing reference output produces original input."""
        input_data = vector_data["input_data"]
        expected_output = vector_data["expected_output"]
        packet_size_bits = vector_data["packet_length"] * 8

        # Decompress the reference compressed output
        decompressed = decompress(
            expected_output,
            packet_size=packet_size_bits,
            robustness=vector_data["robustness"],
        )

        assert decompressed == input_data, (
            f"Decompression of reference failed for {vector_data['name']}: "
            f"expected {len(input_data)} bytes, got {len(decompressed)} bytes"
        )
